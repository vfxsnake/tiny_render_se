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

void LineDrawer::drawLineNaive(Vec2i a, Vec2i b, Color color, Framebuffer& frame_buffer)
{
    bool y_major = toShallowLeftToRight(a, b);
    
    // recompute deltas as they could have been swapped
    int delta_x = b.x - a.x;
    if (delta_x == 0)
    {
        // Post-normalization |delta_x| >= |delta_y|, so delta_x == 0 implies delta_y == 0
        // and thus a == b. Degenerate edge: not a line, draws nothing. Guards dividing by 0 for minor_axis_step.
        return;
    } 
        
    
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

void LineDrawer::drawLineAccum(Vec2i a, Vec2i b, Color color, Framebuffer& frame_buffer)
{
    bool y_major = toShallowLeftToRight(a, b);
    
    // recompute deltas as they could have been swapped
    int delta_x = b.x - a.x;
    if (delta_x == 0)
    {
        // Post-normalization |delta_x| >= |delta_y|, so delta_x == 0 implies delta_y == 0
        // and thus a == b. Degenerate edge: not a line, draws nothing. Guards dividing by 0 for minor_axis_step.
        return;
    } 
        
    
    int delta_y = b.y - a.y;
    int minor_direction = (delta_y >= 0) ? 1: -1;

    float minor_axis_step = float(delta_y) / float(delta_x);
    float minor_step_accumulation = 0.0f;

    for (int i = 0; i <= delta_x; i++)
    {
        if (y_major)
        {
            frame_buffer.setPixel(a.y + std::round(minor_step_accumulation), a.x + i, color);    
        }
        
        else
        {
            frame_buffer.setPixel(a.x + i, a.y + std::round(minor_step_accumulation), color);
        }
        minor_step_accumulation += minor_axis_step;
    } 
}


/*
    Implementation of the Bresenham draw line algorithm.   
*/
void LineDrawer::drawLine(Vec2i a, Vec2i b, Color color, Framebuffer& frame_buffer)
{
    bool y_major = toShallowLeftToRight(a, b);
    
    // recompute deltas as they could have been swapped
    int delta_x = b.x - a.x;
    if (delta_x == 0) // same pixel position, nothing to do.
    {
        return;
    } 
        
    int delta_y = b.y - a.y;
    int minor_direction = (delta_y >= 0) ? 1: -1;
    int minor = a.y;
    int minor_axis_increment = 2 * std::abs(delta_y);
    int error = 0;
    int error_carry = 2 * delta_x;

    for (int i = 0; i <= delta_x; i++)
    {
        if (y_major)
        {
            frame_buffer.setPixel(minor , a.x + i, color);    
        }
        
        else
        {
            frame_buffer.setPixel(a.x + i, minor , color);
        }

        error += minor_axis_increment;
        if (error >= delta_x)
        {
            minor += minor_direction;
            error -= error_carry;
        }
    } 
}