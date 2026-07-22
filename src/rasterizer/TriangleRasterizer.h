#pragma once

#include "primitives/Triangle.h"
#include "Color.h"

// forward declarations
class Framebuffer;

namespace TriangleRasterizer
{

    void drawTriangleScanline(Triangle triangle, Color color, Framebuffer& frame_buffer);


} // end of TriangleRasterizer name space