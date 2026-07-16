#include "LineDrawer.h"

#include <utility>
#include <cstdlib>
#include <cmath>
#include "Framebuffer.h"


namespace
{
    [[nodiscard]]
    bool toShallowLeftToRight(Vec2i& a, Vec2i& b)
    {
        bool y_major = false;

        int delta_x = b.x - a.x;
        int delta_y = b.y - a.y;

        if (std::abs(delta_y) > std::abs(delta_x))
        {
            std::swap(a.x, a.y);
            std::swap(b.x, b.y);
            y_major = true;
        }

        if (a.x > b.x) 
        {
            std::swap(a, b);
        }

        return y_major;
    }
}

void LineDrawer::drawLine(Vec2i a, Vec2i b, Color color, Framebuffer& frame_buffer)
{
    bool y_major = toShallowLeftToRight(a, b);
    
    // recompute deltas as they could have been swapped
    int delta_x = b.x - a.x;
    int delta_y = b.y - a.y;

    float minor_axis_step = float(delta_y) / float(delta_x);
    

    for (int i = 0; i <= delta_x; i++)
    {
        if (y_major)
        {
            frame_buffer.setPixel(a.y + std::round(minor_axis_step * i), a.x + i, color);    
        }
        
        else
        {
            frame_buffer.setPixel(a.x + i, a.y + std::round(minor_axis_step * i), color);
        }
    }   
}
