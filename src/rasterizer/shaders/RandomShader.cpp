#include "RandomShader.h"

#include "geometry/Mesh.h"


RandomShader::RandomShader(const Mesh& mesh, const tinymath::Matrix4x4& transform) :
    mesh_(&mesh), 
    transform_(transform)
{
    randomNumberGenerator_.seed(131517);
}


tinymath::Vec4f RandomShader::vertex(int face_index, int vertex_index)
{
    const auto& vertex_list =  mesh_->faceIndices[face_index];
    tinymath::Vec4f transformed_vertex = transform_ * tinymath::toVec4(mesh_->vertices[vertex_list[vertex_index]]);

    if (vertex_index == 0)
    {
        faceColor_ = {
            static_cast<uint8_t>(distribution_(randomNumberGenerator_)),
            static_cast<uint8_t>(distribution_(randomNumberGenerator_)),
            static_cast<uint8_t>(distribution_(randomNumberGenerator_)),
            255
        };
    }

    return transformed_vertex;
}


bool RandomShader::fragment(screen::BarycentricWeights, Color& out_color)
{
    out_color = faceColor_;
    return true;
}
