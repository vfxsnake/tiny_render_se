#pragma once


#include "AbstractShader.h"
#include "math/Matrix4x4.h"

// forward declaration
struct Mesh;


/*
    Depth shader, fills only  the depth buffer.
*/
class DepthShader : public AbstractShader
{
public:
    DepthShader(
        const Mesh& mesh,
        const tinymath::Matrix4x4& transform
    );

    tinymath::Vec4f vertex(int face_index, int vertex_index) override;
    bool fragment(screen::BarycentricWeights weights, Color& out_color) override;

private:
    const Mesh* mesh_;
    tinymath::Matrix4x4 transform_;

};