#include "BlinnPhongShader.h"

#include <algorithm>
#include <cmath>

#include "geometry/Mesh.h"

BlinnPhongShader::BlinnPhongShader(
        const Mesh& mesh,
        const tinymath::Matrix4x4& transform,
        tinymath::Vec3f light_direction,
        tinymath::Vec3f view_direction,
        Color base_color,
        Color specular_color,
        float ambient,
        float shininess
) :
    mesh_(&mesh),
    transform_(transform),
    lightDirection_(tinymath::normalize(light_direction)),
    viewDirection_(tinymath::normalize(view_direction)),
    baseColor_(base_color),
    specularColor_(specular_color),
    ambient_(ambient),
    shininess_(shininess)
{

}


tinymath::Vec4f BlinnPhongShader::vertex(int face_index, int vertex_index)
{
    const auto& vertex_list = mesh_->faceIndices[face_index];
    tinymath::Vec3f vertex_position = mesh_->vertices[vertex_list[vertex_index]];
    
    const auto& normal_list = mesh_->faceNormalIndices[face_index];
    varyingNormals_[vertex_index] = mesh_->normals[normal_list[vertex_index]];
    
    tinymath::Vec4f transformed_vertex = transform_ * tinymath::toVec4(vertex_position);

    return transformed_vertex;
}


bool BlinnPhongShader::fragment(screen::BarycentricWeights weights, Color& out_color)
{
    tinymath::Vec3f current_normal = tinymath::normalize(
        varyingNormals_[0] * weights.alpha + 
        varyingNormals_[1] * weights.beta +
        varyingNormals_[2] * weights.gamma
    );

    float light_normal_incident_ratio = tinymath::dot(current_normal, lightDirection_);

    /*
        computing the view_direction and light_direction middle (half) vector.
    */
    tinymath::Vec3f view_light_half_vector = tinymath::normalize(lightDirection_ + viewDirection_);

    // diffuse component
    float diffuse = std::max(0.0f, light_normal_incident_ratio);
        
    // specular component, calculation using the view-light half or center vector.
    float specular = std::pow(std::max(0.0f, tinymath::dot(current_normal, view_light_half_vector)), shininess_);

    float red = static_cast<float>(baseColor_.r);
    float green = static_cast<float>(baseColor_.g);
    float blue = static_cast<float>(baseColor_.b);

    float specular_red = static_cast<float>(specularColor_.r);
    float specular_green = static_cast<float>(specularColor_.g);
    float specular_blue = static_cast<float>(specularColor_.b);
    
    out_color = {
        static_cast<uint8_t>(std::min(red * diffuse + specular_red * specular + red * ambient_, 255.0f)),
        static_cast<uint8_t>(std::min(green * diffuse + specular_green * specular + green * ambient_, 255.0f)),
        static_cast<uint8_t>(std::min(blue * diffuse + specular_blue * specular + blue * ambient_, 255.0f)),
        baseColor_.a
    };
    
    return true;
}