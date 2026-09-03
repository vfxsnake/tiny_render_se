#pragma once

#include <array>

#include "AbstractShader.h"
#include "math/Matrix4x4.h"
#include "math/Vec3.h"
#include "rasterizer/Color.h"

// forward declaration
struct Mesh;

class GouraudShader : public AbstractShader
{
public:
    GouraudShader(
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
    
    Color baseColor_;
    std::array<float, 3> varyingIntensities_ = {};
};
