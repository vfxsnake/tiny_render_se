#pragma once

#include "math/Vec4.h"
#include "rasterizer/Color.h"
#include "rasterizer/ScreenSpace.h"


/*
    The two-stage contract every shader in this project implements.

    vertex()   runs once per triangle corner and returns that corner in clip
               space; the rasterizer does the perspective divide and the viewport
               map. It is also where a shader stashes whatever fragment() will
               need - the varying data, carried on members.

    fragment() runs once per candidate pixel, given that pixel's barycentric
               weights inside the triangle. Returning false discards the pixel
               entirely: no colour is written and the z-buffer is left untouched,
               so a discarded fragment does not occlude what is behind it.
*/
class AbstractShader
{
public:
    virtual ~AbstractShader() = default;
    virtual tinymath::Vec4f vertex(int face_index, int vertex_index) = 0;
    virtual bool fragment(screen::BarycentricWeights weights, Color& out_color) = 0;
};