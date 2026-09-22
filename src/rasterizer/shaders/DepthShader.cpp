#include "DepthShader.h"

#include "geometry/Mesh.h"


DepthShader::DepthShader(
    const Mesh& mesh,
    const tinymath::Matrix4x4& transform
) :
    mesh_(&mesh),
    transform_(transform)
{

}


tinymath::Vec4f DepthShader::vertex(int face_index, int vertex_index)
{
    const auto& vertex_list =  mesh_->faceIndices[face_index];
    tinymath::Vec3f current_vertex = mesh_->vertices[vertex_list[vertex_index]];
    tinymath::Vec4f transformed_vertex = transform_ * tinymath::toVec4(current_vertex);
    
    return transformed_vertex;
}


bool DepthShader::fragment(screen::BarycentricWeights, Color& out_color)
{
    out_color = {0,0,0,255};
    return true;
}