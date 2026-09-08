#pragma once

#include <array>

#include "AbstractShader.h"
#include "math/Matrix4x4.h"
#include "math/Vec3.h"
#include "rasterizer/Color.h"

// forward declaration
struct Mesh;

/*
    Primitive-rate rung: one intensity per face, so the mesh renders faceted.

    The normal is computed here rather than read from the file's vn. This rung
    exists to show flat shading, and the cross product of the triangle's own
    edges is the only normal guaranteed to be constant across the primitive; the
    vn are per-corner and would defeat the point. GouraudShader is where they
    start being used.

    Ordering contract: the normal needs all three corners, so it is computed on
    corner 2 and the three vertex() calls must arrive in index order.
*/
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
