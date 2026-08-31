#pragma once

#include <array>

#include "AbstractShader.h"
#include "math/Matrix4x4.h"
#include "math/Vec3.h"
#include "rasterizer/Color.h"

// forward declaration
struct Mesh;

class FaceShader : public AbstractShader
{
public:
    FaceShader(
        const Mesh& mesh,
        const tinymath::Matrix4x4& transform,
        tinymath::Vec3f light_direction,
        Color base_color
    );

    tinymath::Vec4f vertex(int face_index, int vertex_index) override;
    bool fragment(screen::BarycentricWeights weights, Color& out_color) override;

private:
    const Mesh* mesh_;
    tinymath::Matrix4x4 transform_;
    tinymath::Vec3f lightDirection_;
    std::array<tinymath::Vec3f, 3> vertexWorldPositions_;
    
    Color baseColor_;
    float faceIntensity_ = 0.0f;
};
