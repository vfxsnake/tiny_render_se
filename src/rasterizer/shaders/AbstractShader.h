#pragma once

#include "math/Vec4.h"
#include "rasterizer/Color.h"
#include "rasterizer/ScreenSpace.h"


class AbstractShader
{
public:
    virtual ~AbstractShader() = default;
    virtual tinymath::Vec4f vertex(int face_index, int vertex_index) = 0;
    virtual bool fragment(screen::BarycentricWeights weights, Color& out_color) = 0;
};