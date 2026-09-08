#pragma once

#include <random>

#include "AbstractShader.h"
#include "math/Matrix4x4.h"
#include "rasterizer/Color.h"

// forward declaration
struct Mesh;

/*
    Plumbing proof, not a material. Gives each face a flat random colour so the
    shader path itself can be verified - vertex() reaching the mesh, fragment()
    reaching the framebuffer, one call pair per triangle - with no lighting to
    confuse a wiring bug for a shading bug. Deliberately unlit.

    The generator is fixed-seeded, so the same mesh always produces the same
    colours; a frame that changes between runs is a bug, not randomness.

    Ordering contract: the colour is rolled on corner 0, so the three vertex()
    calls must arrive in index order.
*/
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
