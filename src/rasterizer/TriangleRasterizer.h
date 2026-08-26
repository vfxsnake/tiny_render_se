#pragma once

#include "primitives/Triangle.h"
#include "Color.h"

// forward declarations
class Framebuffer;


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
        implementation of Barycentric and bounding box algorithm, ideal for multi-threading.
    */
    void drawTriangle(const Triangle& triangle, Color color, Framebuffer& frame_buffer, bool cull_back_faces = true);


} // end of TriangleRasterizer name space
