#pragma once

#include <array>

#include "AbstractShader.h"
#include "math/Matrix4x4.h"
#include "math/Vec2.h"
#include "rasterizer/Color.h"

// forward declaration
struct Mesh;
class Texture;

/*
    TextureShader is a constant (flat) shader, 
    samples the texture color using the uv coordinates.
*/
class TextureShader : public AbstractShader
{
public:
    TextureShader(
        const Mesh& mesh,
        const Texture& texture,
        const tinymath::Matrix4x4& transform
    );

    tinymath::Vec4f vertex(int face_index, int vertex_index) override;
    bool fragment(screen::BarycentricWeights weights, Color& out_color) override;

private:
    const Mesh* mesh_;
    const Texture* diffuseTexture_;
    tinymath::Matrix4x4 transform_;
    std::array<tinymath::Vec2f, 3> varyingUvs_ = {};
    
};