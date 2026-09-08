#pragma once

#include <array>

#include "AbstractShader.h"
#include "math/Matrix4x4.h"
#include "math/Vec3.h"
#include "rasterizer/Color.h"

// forward declaration
struct Mesh;

/*
    Vertex-rate rung: lighting is evaluated in vertex(), once per corner from the
    file's vn, and the three resulting intensities are blended across the
    triangle. Faceting dissolves into a gradient while genuine hard edges stay
    hard, because a hard edge carries a separate vn per face.

    This is not a wrong implementation - hardware shaded this way for years. It
    is superseded by what it cannot represent: an effect that peaks in the middle
    of a triangle is evaluated at no corner, so it does not exist at all. A
    specular highlight landing mid-face is the case that forces per-pixel
    lighting.

    No ordering contract - each corner writes only its own slot, so the vertex()
    calls may arrive in any order.
*/
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
