#pragma once

#include <random>

#include "AbstractShader.h"
#include "math/Matrix4x4.h"
#include "rasterizer/Color.h"

// forward declaration
struct Mesh;

class RandomShader : public AbstractShader
{
public:
    RandomShader(const Mesh& mesh, const tinymath::Matrix4x4& transform);

    tinymath::Vec4f vertex(int face_index, int vertex_index) override;
    bool fragment(screen::BarycentricWeights weights, Color& out_color) override;

private:
    const Mesh* mesh_;
    tinymath::Matrix4x4 transform_;
    Color faceColor_;
    std::mt19937 randomNumberGenerator_;
    std::uniform_int_distribution<int> distribution_{0, 255};
};
