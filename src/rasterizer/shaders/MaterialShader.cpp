#include "MaterialShader.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "geometry/Mesh.h"
#include "rasterizer/Texture.h"
#include "rasterizer/Framebuffer.h"
#include "AmbientOcclusion.h"


MaterialShader::MaterialShader(
        const Mesh& mesh,
        const Framebuffer& shadow_map_buffer,
        const Framebuffer& camera_depth_buffer,
        const Texture& diffuse_texture,
        const Texture& specular_texture,
        const Texture& emission_texture,
        const Texture& normal_map_texture,
        const tinymath::Matrix4x4& transform,
        const tinymath::Matrix4x4& shadow_lookup_transform,
        const tinymath::Matrix4x4& ambient_occlusion_lookup_transform,
        tinymath::Vec3f light_direction,
        tinymath::Vec3f view_direction,
        float diffuse_intensity,
        float specular_intensity,
        float shininess,
        float emission_intensity,
        float ambient_intensity,
        float shadow_bias,
        int occlusion_sample_count,
        float occlusion_radius,
        float occlusion_bias
) :
    mesh_(&mesh),
    shadowMapFramebuffer_(&shadow_map_buffer),
    cameraDepthFramebuffer_(&camera_depth_buffer),
    diffuseTexture_(&diffuse_texture),
    specularTexture_(&specular_texture),
    emissionTexture_(&emission_texture),
    normalMapTexture_(&normal_map_texture),
    transform_(transform),
    shadowLookupTransform_(shadow_lookup_transform),
    ambientOcclusionLookupTransform_(ambient_occlusion_lookup_transform),
    lightDirection_(tinymath::normalize(light_direction)),
    viewDirection_(tinymath::normalize(view_direction)),
    diffuseIntensity_(diffuse_intensity),
    specularIntensity_(specular_intensity),
    shininess_(shininess),
    emissionIntensity_(emission_intensity),
    ambientIntensity_(ambient_intensity),
    shadowBias_(shadow_bias),
    occlusionRadius_(occlusion_radius),
    occlusionBias_(occlusion_bias)
    
{
    std::mt19937 gen(735);

    // defining the range
    std::uniform_real_distribution<float> distribution{-1.0f, 1.0f};
    
    while (static_cast<int>(occlusionSampleVectors_.size()) < occlusion_sample_count)
    {
        tinymath::Vec3f sample_vector{
            distribution(gen),
            distribution(gen),
            distribution(gen)
        };

        if (tinymath::dot(sample_vector, sample_vector) <= 1.0f)
        {
            occlusionSampleVectors_.push_back(sample_vector);
        }
    }
}


tinymath::Vec4f MaterialShader::vertex(int face_index, int vertex_index)
{
    const auto& vertex_list = mesh_->faceIndices[face_index];
    vertexWorldPosition_[vertex_index] = mesh_->vertices[vertex_list[vertex_index]];
    
    const auto& normal_list = mesh_->faceNormalIndices[face_index];
    varyingNormals_[vertex_index] = mesh_->normals[normal_list[vertex_index]];

    const auto& uv_coord_list = mesh_->faceTextureCoordinateIndices[face_index];
    tinymath::Vec3f uvw = mesh_->textureCoordinates[uv_coord_list[vertex_index]];
    varyingUvs_[vertex_index] = {uvw.x, uvw.y};
    
    tinymath::Vec4f transformed_vertex = transform_ * tinymath::toVec4(vertexWorldPosition_[vertex_index]);

    return transformed_vertex;
}


bool MaterialShader::fragment(screen::BarycentricWeights weights, Color& out_color)
{
    tinymath::Vec2f uv_value = varyingUvs_[0] * weights.alpha + 
                               varyingUvs_[1] * weights.beta +
                               varyingUvs_[2] * weights.gamma;
    
    Color sampled_normal_color  = normalMapTexture_->sample(uv_value.x, uv_value.y);
    float sampled_normal_x = ((static_cast<float>(sampled_normal_color.r) / 255.0f) * 2) - 1;
    float sampled_normal_y = ((static_cast<float>(sampled_normal_color.g) / 255.0f) * 2) - 1;
    float sampled_normal_z = ((static_cast<float>(sampled_normal_color.b) / 255.0f) * 2) - 1;

    tinymath::Vec3f sampled_normal = tinymath::normalize(tinymath::Vec3f(sampled_normal_x, sampled_normal_y, sampled_normal_z));
    
    // resolving the tangent space normals
    // building triangle edges
    tinymath::Vec3f edge_0 = vertexWorldPosition_[1] - vertexWorldPosition_[0];
    tinymath::Vec3f edge_1 = vertexWorldPosition_[2] - vertexWorldPosition_[0];
    
    // getting uv deltas for the same edges
    tinymath::Vec2f delta_uv_0 = varyingUvs_[1] - varyingUvs_[0];
    tinymath::Vec2f delta_uv_1 = varyingUvs_[2] - varyingUvs_[0];

    // calculating the determinant
    float determinant = tinymath::cross(delta_uv_0, delta_uv_1);
    float one_over_determinant = 1.0f / determinant;
    tinymath::Vec3f t = tinymath::normalize((edge_0 * delta_uv_1.y - edge_1 * delta_uv_0.y) * one_over_determinant);
    tinymath::Vec3f b = tinymath::normalize((edge_1 * delta_uv_0.x - edge_0 * delta_uv_1.x) * one_over_determinant);

    // calculate the interpolated normal
    tinymath::Vec3f interpolated_normal = tinymath::normalize(
        varyingNormals_[0] * weights.alpha + 
        varyingNormals_[1] * weights.beta +
        varyingNormals_[2] * weights.gamma
    );

    // combining all together
    tinymath::Vec3f current_normal = tinymath::normalize( 
        t * sampled_normal.x +
        b * sampled_normal.y +
        interpolated_normal * sampled_normal.z
    );

    float light_normal_incident_ratio = tinymath::dot(current_normal, lightDirection_);

    /*
        computing the view_direction and light_direction middle (half) vector.
    */
    tinymath::Vec3f view_light_half_vector = tinymath::normalize(lightDirection_ + viewDirection_);

    // diffuse component
    float diffuse = std::max(0.0f, light_normal_incident_ratio) * diffuseIntensity_;
        
    // specular component, calculation using the view-light half or center vector.
    float specular = std::pow(std::max(0.0f, tinymath::dot(current_normal, view_light_half_vector)), shininess_) * specularIntensity_;

    
    Color diffuse_color = diffuseTexture_->sample(uv_value.x, uv_value.y);
    Color specular_color = specularTexture_->sample(uv_value.x, uv_value.y);
    Color emission_color = emissionTexture_->sample(uv_value.x, uv_value.y);

    float diffuse_red = static_cast<float>(diffuse_color.r);
    float diffuse_green = static_cast<float>(diffuse_color.g);
    float diffuse_blue = static_cast<float>(diffuse_color.b);

    float specular_red = static_cast<float>(specular_color.r);
    float specular_green = static_cast<float>(specular_color.g);
    float specular_blue = static_cast<float>(specular_color.b);
    
    float emission_red = static_cast<float>(emission_color.r);
    float emission_green = static_cast<float>(emission_color.g);
    float emission_blue = static_cast<float>(emission_color.b);

    // shadow map lookup
    tinymath::Vec3f interpolated_position = vertexWorldPosition_[0] * weights.alpha +
                                            vertexWorldPosition_[1] * weights.beta +
                                            vertexWorldPosition_[2] * weights.gamma;

    tinymath::Vec3f shadow_point_transformed = tinymath::toVec3(
        shadowLookupTransform_ * tinymath::toVec4(interpolated_position)
    );
    
    float shadow_depth_value = shadowMapFramebuffer_->getDepth(
        static_cast<int>(std::round(shadow_point_transformed.x)),
        static_cast<int>(std::round(shadow_point_transformed.y))
    );
    
    float shadow_multiplier = 1.0f;
    
    if ((shadow_point_transformed.z + shadowBias_ )<= shadow_depth_value)
    {
        shadow_multiplier = 0.0f;
    }

    // Ambient Occlusion estimation
    float ambient_occlusion = AmbientOcclusion::estimateAmbientOcclusion(
        interpolated_position,
        current_normal,
        occlusionSampleVectors_,
        occlusionRadius_,
        occlusionBias_,
        ambientOcclusionLookupTransform_,
        *cameraDepthFramebuffer_
    );

    out_color = {
        static_cast<uint8_t>( // out color Red
            std::min(
                diffuse_red * diffuse * shadow_multiplier + 
                specular_red * specular * shadow_multiplier + 
                emission_red * emissionIntensity_ +
                diffuse_red * ambientIntensity_ * ambient_occlusion, 
                255.0f
            )
        ), 
        static_cast<uint8_t>( // out color Green
            std::min(
                diffuse_green * diffuse * shadow_multiplier + 
                specular_green * specular * shadow_multiplier + 
                emission_green * emissionIntensity_ +
                diffuse_green * ambientIntensity_ * ambient_occlusion, 
                255.0f
            )
        ), 
        static_cast<uint8_t>( // out color Blue
            std::min(
                diffuse_blue * diffuse * shadow_multiplier +
                specular_blue * specular * shadow_multiplier + 
                emission_blue * emissionIntensity_ +
                diffuse_blue * ambientIntensity_ * ambient_occlusion, 
                255.0f
            )
        ), 
        255  //out color Alpha
    };
    
    return true;
}
