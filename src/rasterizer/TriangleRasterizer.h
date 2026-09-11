#pragma once

#include <array>

#include "primitives/Triangle.h"
#include "Color.h"
#include "math/Vec4.h"

// forward declarations
class Framebuffer;
class AbstractShader;

namespace TriangleRasterizer
{
    /*
        Old rasterization procedure, using the scan line algorithm, faster for single 
        thread but, lacks of support for multi-theading, uses the deprecated Triangle2D
    */
    void drawTriangleScanline(const Triangle2D& triangle, Color color, Framebuffer& frame_buffer);

    /* 
        Old implementation of Barycentric and bounding box algorithm using the deprecated Triangle2D. 
    */
    void drawTriangle2D(const Triangle2D& triangle, Color color, Framebuffer& frame_buffer);

    /*
        Barycentric and bounding box algorithm with one constant colour per triangle, taking a
        triangle already in screen space. Superseded by drawTriangle, kept as the standing A/B
        and benchmark baseline for the shader path: same coverage, depth and culling rules,
        with no shader call per pixel.
    */
    void drawTriangleSolidColor(const Triangle& triangle, Color color, Framebuffer& frame_buffer, bool cull_back_faces = true);

    /*
        The shaded path. Takes the three corners in clip space, as returned by
        AbstractShader::vertex(), and owns everything between the two shader stages: the
        perspective divide, the viewport map to this framebuffer's size, the degenerate and
        back-face checks, the bounding box, and the per-pixel coverage and depth tests.

        The caller must run shader.vertex() for all three corners of this triangle before
        calling, in the order the shader's ordering contract requires. This function never
        calls vertex(); the varyings fragment() reads were written by those calls.

        The depth test runs before fragment(), so a pixel already hidden by nearer geometry
        never invokes the shader. A fragment() returning false writes neither colour nor
        depth, so a discarded fragment cannot occlude anything drawn after it.

        Known limits: there is no clipping, so a corner with w == 0 trips the assert in
        toVec3 and a corner with w < 0 (behind the eye) produces garbage; and the weights
        passed to fragment() are screen-space, not perspective-correct.
    */
    void drawTriangle(
        const std::array<tinymath::Vec4f, 3>& clip_positions,
        AbstractShader& shader,
        Framebuffer& frame_buffer,
        bool cull_back_faces = true
    );

} // end of TriangleRasterizer name space
