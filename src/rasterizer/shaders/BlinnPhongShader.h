#pragma once

#include <array>

#include "AbstractShader.h"
#include "math/Matrix4x4.h"
#include "math/Vec3.h"
#include "rasterizer/Color.h"

// forward declaration
struct Mesh;

/*
    Shader derived from the Phong model, using view-light center vector. 
*/
class BlinnPhongShader : public AbstractShader
{
public:
    BlinnPhongShader(
        const Mesh& mesh,
        const tinymath::Matrix4x4& transform,
        tinymath::Vec3f light_direction,
        tinymath::Vec3f view_direction,
        Color base_color,
        Color specular_color,
        float ambient,
        float shininess
    );

    tinymath::Vec4f vertex(int face_index, int vertex_index) override;
    bool fragment(screen::BarycentricWeights weights, Color& out_color) override;

private:
    const Mesh* mesh_;
    tinymath::Matrix4x4 transform_;
    tinymath::Vec3f lightDirection_;
    tinymath::Vec3f viewDirection_;
    
    Color baseColor_;
    Color specularColor_;
    float ambient_;
    float shininess_;
    std::array<tinymath::Vec3f, 3> varyingNormals_ = {};
};
