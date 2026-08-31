#include "FaceShader.h"

#include "geometry/Mesh.h"

FaceShader::FaceShader(
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

tinymath::Vec4f FaceShader::vertex(int face_index, int vertex_index)
{
    const auto& vertex_list =  mesh_->faceIndices[face_index];
    vertexWorldPositions_[vertex_index] = mesh_->vertices[vertex_list[vertex_index]];
    
    if (vertex_index == 2)
    {
        tinymath::Vec3f segment_ab = vertexWorldPositions_[1] - vertexWorldPositions_[0];
        tinymath::Vec3f segment_ac = vertexWorldPositions_[2] - vertexWorldPositions_[0];

        tinymath::Vec3f face_normal = tinymath::normalize(tinymath::cross(segment_ab, segment_ac));
        faceIntensity_ = tinymath::dot(face_normal, lightDirection_);
    }
    tinymath::Vec4f transformed_vertex = transform_ * tinymath::toVec4(vertexWorldPositions_[vertex_index]);
    
    return transformed_vertex;
}

bool FaceShader::fragment(screen::BarycentricWeights weights, Color& out_color)
{
    
}