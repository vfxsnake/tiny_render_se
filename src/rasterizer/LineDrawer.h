#pragma once

#include "math/Vec2.h"
#include "Color.h"


// forward declaration
class Framebuffer;

namespace LineDrawer
{
    void drawLineNaive(Vec2i a, Vec2i b, Color color, Framebuffer& frame_buffer);

    void drawLineAccum(Vec2i a, Vec2i b, Color color, Framebuffer& frame_buffer);

    void drawLine(Vec2i a, Vec2i b, Color color, Framebuffer& frame_buffer);
    
} // end of LineDrawer namespace
