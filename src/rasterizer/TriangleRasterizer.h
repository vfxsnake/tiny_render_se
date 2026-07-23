#pragma once

#include "primitives/Triangle.h"
#include "Color.h"

// forward declarations
class Framebuffer;

namespace TriangleRasterizer
{
    /*
        Old rasterization procedure, using the scan line algorithm, faster for single 
        thread but, lacks of support for multi-theading
    */
    void drawTriangleScanline(Triangle triangle, Color color, Framebuffer& frame_buffer);

    /*
        implementation of Barycentric and bounding box algorithm. ideal for multi-threading.
    */
    void drawTriangle(Triangle triangle, Color color, Framebuffer& frame_buffer);


} // end of TriangleRasterizer name space