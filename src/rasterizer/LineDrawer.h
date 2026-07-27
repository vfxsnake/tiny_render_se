#pragma once

#include "math/Vec2.h"
#include "Color.h"


// forward declaration
class Framebuffer;

namespace LineDrawer
{
    void drawLineNaive(tinymath::Vec2i a, tinymath::Vec2i b, Color color, Framebuffer& frame_buffer);

    void drawLineAccum(tinymath::Vec2i a, tinymath::Vec2i b, Color color, Framebuffer& frame_buffer);

    void drawLine(tinymath::Vec2i a, tinymath::Vec2i b, Color color, Framebuffer& frame_buffer);
    
} // end of LineDrawer namespace
