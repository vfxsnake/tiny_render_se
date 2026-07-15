#include "LineDrawer.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include "Framebuffer.h"

#include <iostream>

void LineDrawer::drawLine(Vec2i a, Vec2i b, Color color, Framebuffer& frame_buffer)
{
    bool y_major = false;
    bool swap_order = false;

    int delta_x = b.x - a.x;
    int delta_y = b.y - a.y;


    int max_steps = std::max(abs(delta_y),abs(delta_x));

    if (abs(delta_y) > abs(delta_x))
    {
        std::swap(a.x, a.y);
        std::swap(b.x, b.y);
        y_major = true;
    }

    if (a.x > b.x) 
    {
        std::swap(a, b);
        swap_order = true;
    }

    float minor_axis_step;
    if (y_major)
    {
        minor_axis_step = float(delta_x) / float(max_steps); 
    }
    else
    {
        minor_axis_step = float(delta_y) / float(max_steps);
    }

    int direction = 1;

    for (int i = 0; i < max_steps; i++)
    {

        if (swap_order)
        {
            std::swap(b, a); 
            swap_order = false;
            direction = -1;
        }
        
        if (y_major)
        {
            frame_buffer.setPixel(a.y + round(minor_axis_step * i), a.x + i * direction, color);    
        }
        
        else
        {
            frame_buffer.setPixel(a.x + i * direction , a.y + round(minor_axis_step * i), color);
        }
    }
    

    
}