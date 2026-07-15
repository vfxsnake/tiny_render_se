#pragma once

#include "math/Vec2.h"
#include "Color.h"


// forward declaration
class Framebuffer;

namespace LineDrawer
{
    void drawLine(Vec2i a, Vec2i b, Color color, Framebuffer& frame_buffer);

} // end of LinearDrawer namespace
