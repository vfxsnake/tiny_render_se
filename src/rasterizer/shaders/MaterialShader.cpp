#include "MaterialShader.h"

#include <algorithm>
#include <cmath>

#include "geometry/Mesh.h"
#include "rasterizer/Texture.h"


MaterialShader::MaterialShader(
        const Mesh& mesh,
        const Texture& diffuse_texture,
        const Texture& specular_texture,
        const Texture& emission_texture,
        const Texture& normal_map_texture,
        const tinymath::Matrix4x4& transform,
        tinymath::Vec3f light_direction,
        tinymath::Vec3f view_direction,
        float diffuse_intensity,
        float specular_intensity,
        float shininess,
        float emission_intensity,
        float ambient_intensity
) :
    mesh_(&mesh),
    diffuseTexture_(&diffuse_texture),
    specularTexture_(&specular_texture),
    emissionTexture_(&emission_texture),
    normalMapTexture_(&normal_map_texture),
    transform_(transform),
    lightDirection_(tinymath::normalize(light_direction)),
    viewDirection_(tinymath::normalize(view_direction)),
    diffuseIntensity_(diffuse_intensity),
    specularIntensity_(specular_intensity),
    shininess_(shininess),
    emissionIntensity_(emission_intensity),
    ambientIntensity_(ambient_intensity)

{

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

    out_color = {
        static_cast<uint8_t>( // out color Red
            std::min(
                diffuse_red * diffuse + 
                specular_red * specular + 
                emission_red * emissionIntensity_ +
                diffuse_red * ambientIntensity_, 
                255.0f
            )
        ), 
        static_cast<uint8_t>( // out color Green
            std::min(
                diffuse_green * diffuse + 
                specular_green * specular + 
                emission_green * emissionIntensity_ +
                diffuse_green * ambientIntensity_, 
                255.0f
            )
        ), 
        static_cast<uint8_t>( // out color Blue
            std::min(
                diffuse_blue * diffuse +
                specular_blue * specular + 
                emission_blue * emissionIntensity_ +
                diffuse_blue * ambientIntensity_, 
                255.0f
            )
        ), 
        255  //out color Alpha
    };
    
    return true;
}
