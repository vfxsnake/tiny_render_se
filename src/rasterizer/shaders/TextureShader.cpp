#include "TextureShader.h"

#include "geometry/Mesh.h"
#include "rasterizer/Texture.h"
#include "math/Vec3.h"

TextureShader::TextureShader(
    const Mesh& mesh,
    const Texture& texture,
    const tinymath::Matrix4x4& transform
): 
    mesh_(&mesh),
    diffuseTexture_(&texture),
    transform_(transform)
{

}

tinymath::Vec4f TextureShader::vertex(int face_index, int vertex_index)
{
    const auto& vertex_list = mesh_->faceIndices[face_index];
    tinymath::Vec3f vertex_position = mesh_->vertices[vertex_list[vertex_index]];
    
    const auto& uv_coord_list = mesh_->faceTextureCoordinateIndices[face_index];
    
    tinymath::Vec3f uvw = mesh_->textureCoordinates[uv_coord_list[vertex_index]];
    varyingUvs_[vertex_index] = {uvw.x, uvw.y};

    tinymath::Vec4f transformed_vertex = transform_ * tinymath::toVec4(vertex_position);

    return transformed_vertex;
}


bool TextureShader::fragment(screen::BarycentricWeights weights, Color& out_color)
{
    tinymath::Vec2f uv_value = varyingUvs_[0] * weights.alpha + 
                               varyingUvs_[1] * weights.beta +
                               varyingUvs_[2] * weights.gamma;

    out_color = diffuseTexture_->sample(uv_value.x, uv_value.y);
    
    return true;
}