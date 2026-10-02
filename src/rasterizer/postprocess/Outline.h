#pragma once

#include "rasterizer/Color.h"

// Forward declaration
class Framebuffer;


namespace Outline
{
    void drawOutlines(Framebuffer& frame_buffer, Color outline_color, float threshold);

} // namespace Outline
