#pragma once

#include <array>

#include "AbstractShader.h"
#include "math/Matrix4x4.h"
#include "math/Vec2.h"
#include "rasterizer/Color.h"

// forward declaration
struct Mesh;

/*
    Utility shader, reads uvs and represent it as color.
*/
class UvColorShader : public AbstractShader
{
public:
    UvColorShader(
        const Mesh& mesh,
        const tinymath::Matrix4x4& transform
    );

    tinymath::Vec4f vertex(int face_index, int vertex_index) override;
    bool fragment(screen::BarycentricWeights weights, Color& out_color) override;

private:
    const Mesh* mesh_;
    tinymath::Matrix4x4 transform_;
    std::array<tinymath::Vec2f, 3> varyingUvs_ = {};
    
};