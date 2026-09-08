#include "LambertShader.h"

#include <algorithm>

#include "geometry/Mesh.h"

LambertShader::LambertShader(
    const Mesh& mesh,
    const tinymath::Matrix4x4& transform,
    tinymath::Vec3f light_direction,
    Color base_color
) :
    mesh_(&mesh),
    transform_(transform),
    lightDirection_(tinymath::normalize(light_direction)),
    baseColor_(base_color)
{
    
}

tinymath::Vec4f LambertShader::vertex(int face_index, int vertex_index)
{
    const auto& vertex_list = mesh_->faceIndices[face_index];
    tinymath::Vec3f vertex_position = mesh_->vertices[vertex_list[vertex_index]];
    
    const auto& normal_list = mesh_->faceNormalIndices[face_index];
    varyingNormals_[vertex_index] = mesh_->normals[normal_list[vertex_index]];
    
    tinymath::Vec4f transformed_vertex = transform_ * tinymath::toVec4(vertex_position);

    return transformed_vertex;
}

bool LambertShader::fragment(screen::BarycentricWeights weights, Color& out_color)
{
    tinymath::Vec3f current_normal = varyingNormals_[0] * weights.alpha + 
                                     varyingNormals_[1] * weights.beta +
                                     varyingNormals_[2] * weights.gamma;

    float intensity = std::max(0.0f, tinymath::dot(tinymath::normalize(current_normal), lightDirection_));
    float red = static_cast<float>(baseColor_.r) * intensity;
    float green = static_cast<float>(baseColor_.g) * intensity;
    float blue = static_cast<float>(baseColor_.b) * intensity;
    
    out_color = {
        static_cast<uint8_t>(red),
        static_cast<uint8_t>(green),
        static_cast<uint8_t>(blue),
        baseColor_.a
    };
    
    return true;
}
