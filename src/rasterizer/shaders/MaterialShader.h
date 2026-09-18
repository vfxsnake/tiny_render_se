#pragma once

#include <array>

#include "AbstractShader.h"
#include "math/Matrix4x4.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "math/Vec4.h"
#include "rasterizer/Color.h"

// forward declaration
struct Mesh;
class Texture;

/*
    Shader derived from the BlinPhong model, using view-light center vector.
    the material properties come from the 3 texture maps required, diffuse, specular and emission,
    additional multipliers are available for controlling the contribution of each component. 
*/
class MaterialShader : public AbstractShader
{
public:
    MaterialShader(
        const Mesh& mesh,
        const Texture& diffuse_texture,
        const Texture& specular_texture,
        const Texture& emission_texture,
        const Texture& normal_map_texture,
        const tinymath::Matrix4x4& transform,
        tinymath::Vec3f light_direction,
        tinymath::Vec3f view_direction,
        float diffuse_intensity,
        float specular_intensity,
        float shininess,
        float emission_intensity,
        float ambient_intensity
    );

    tinymath::Vec4f vertex(int face_index, int vertex_index) override;
    bool fragment(screen::BarycentricWeights weights, Color& out_color) override;

private:
    const Mesh* mesh_;
    
    const Texture* diffuseTexture_;
    const Texture* specularTexture_;
    const Texture* emissionTexture_;
    const Texture* normalMapTexture_;

    tinymath::Matrix4x4 transform_;
    tinymath::Vec3f lightDirection_;
    tinymath::Vec3f viewDirection_;
    
    float diffuseIntensity_;
    float specularIntensity_;
    float shininess_;
    float emissionIntensity_;
    float ambientIntensity_;

    std::array<tinymath::Vec3f, 3> vertexWorldPosition_ = {};
    std::array<tinymath::Vec3f, 3> varyingNormals_ = {};
    std::array<tinymath::Vec2f, 3> varyingUvs_ = {};
};
