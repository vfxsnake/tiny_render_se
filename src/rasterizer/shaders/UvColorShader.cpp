#include "UvColorShader.h"

#include <algorithm>

#include "geometry/Mesh.h"
#include "math/Vec3.h"

UvColorShader::UvColorShader(
    const Mesh& mesh,
    const tinymath::Matrix4x4& transform
) :
    mesh_(&mesh),
    transform_(transform)
{
    
}

tinymath::Vec4f UvColorShader::vertex(int face_index, int vertex_index)
{
    const auto& vertex_list = mesh_->faceIndices[face_index];
    tinymath::Vec3f vertex_position = mesh_->vertices[vertex_list[vertex_index]];
    
    const auto& uv_coord_list = mesh_->faceTextureCoordinateIndices[face_index];
    
    tinymath::Vec3f uvw = mesh_->textureCoordinates[uv_coord_list[vertex_index]];
    varyingUvs_[vertex_index] = {uvw.x, uvw.y};

    tinymath::Vec4f transformed_vertex = transform_ * tinymath::toVec4(vertex_position);

    return transformed_vertex;
}

bool UvColorShader::fragment(screen::BarycentricWeights weights, Color& out_color)
{
    tinymath::Vec2f uv_value = varyingUvs_[0] * weights.alpha + 
                               varyingUvs_[1] * weights.beta +
                               varyingUvs_[2] * weights.gamma;

    float red = 255.0f * uv_value.x;
    float green = 255.0f * uv_value.y;
    float blue = 0.0f;
    
    out_color = {
        static_cast<uint8_t>(red),
        static_cast<uint8_t>(green),
        static_cast<uint8_t>(blue),
        255
    };
    
    return true;
}
