#pragma once

#include <array>
#include <vector>

#include "AbstractShader.h"
#include "math/Matrix4x4.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "math/Vec4.h"
#include "rasterizer/Color.h"

// forward declaration
struct Mesh;
class Texture;
class Framebuffer;

/*
    Toon shader, multi band color implementation.
*/
class ToonShader : public AbstractShader
{
public:
    ToonShader(
        const Mesh& mesh,
        const Framebuffer& shadow_map_buffer,
        const Texture& diffuse_texture,
        const Texture& normal_map_texture,
        const tinymath::Matrix4x4& transform,
        const tinymath::Matrix4x4& shadow_lookup_transform,
        tinymath::Vec3f light_direction,
        tinymath::Vec3f view_direction,
        const std::vector<float>& band_values,
        float specular_threshold,
        float shininess,
        float shadow_bias
    );

    tinymath::Vec4f vertex(int face_index, int vertex_index) override;
    bool fragment(screen::BarycentricWeights weights, Color& out_color) override;

private:
    const Mesh* mesh_;
    
    const Framebuffer* shadowMapFramebuffer_;

    const Texture* diffuseTexture_;
    const Texture* normalMapTexture_;

    tinymath::Matrix4x4 transform_;
    tinymath::Matrix4x4 shadowLookupTransform_;
    tinymath::Vec3f lightDirection_;
    tinymath::Vec3f viewDirection_;
    
    std::vector<float> colorBandValues_;

    float specularThreshold_;
    float shininess_;
    float shadowBias_;
    
    std::array<tinymath::Vec3f, 3> vertexWorldPosition_ = {};
    std::array<tinymath::Vec3f, 3> varyingNormals_ = {};
    std::array<tinymath::Vec2f, 3> varyingUvs_ = {};    
};
