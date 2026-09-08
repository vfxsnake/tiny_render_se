#pragma once

#include <array>

#include "AbstractShader.h"
#include "math/Matrix4x4.h"
#include "math/Vec3.h"
#include "rasterizer/Color.h"

// forward declaration
struct Mesh;

/*
    The first proper material in the series, and the first shader to light per
    pixel. vertex() only carries the corner normals through; fragment() blends
    them, re-normalizes, and evaluates max(0, dot(n, l)) at the pixel.

    The re-normalize is load-bearing: a barycentric blend of three unit vectors
    lands inside the unit sphere, so without it the intensity sags toward
    triangle interiors and the whole surface darkens.

    Against GouraudShader the difference is subtle but real. Gouraud clamps each
    corner to zero before blending, which leaks light past the terminator;
    clamping once at the end puts the terminator where it belongs.

    No ordering contract - each corner writes only its own slot, so the vertex()
    calls may arrive in any order.
*/
class LambertShader : public AbstractShader
{
public:
    LambertShader(
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
    std::array<tinymath::Vec3f, 3> varyingNormals_ = {};
};
