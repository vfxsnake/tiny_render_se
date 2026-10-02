#include "Outline.h"

#include <cmath>
#include <algorithm>
#include <iostream>


#include "rasterizer/Framebuffer.h"

void Outline::drawOutlines(Framebuffer& frame_buffer, Color outline_color, float threshold)
{
    for (int y = 1; y < frame_buffer.getHeight() - 1; y++)
    {
        for (int x = 1; x < frame_buffer.getWidth() - 1; x++)
        {
            float top_x_difference = frame_buffer.getDepth(x - 1, y - 1) - frame_buffer.getDepth(x + 1, y - 1);
            float center_x_difference = frame_buffer.getDepth(x - 1, y) - frame_buffer.getDepth(x + 1, y);
            float bottom_x_difference = frame_buffer.getDepth(x - 1,y + 1) - frame_buffer.getDepth(x + 1, y + 1);
            float gradient_x = top_x_difference + center_x_difference * 2 + bottom_x_difference;
            
            float left_y_difference = frame_buffer.getDepth(x - 1, y - 1) - frame_buffer.getDepth(x - 1, y + 1);
            float center_y_difference = frame_buffer.getDepth(x, y - 1) - frame_buffer.getDepth(x, y + 1);
            float right_y_difference = frame_buffer.getDepth(x + 1, y - 1) - frame_buffer.getDepth(x + 1, y + 1);
            float gradient_y = left_y_difference + center_y_difference * 2 + right_y_difference;

            float edge_strength = std::sqrt(gradient_x * gradient_x + gradient_y * gradient_y);

            if (edge_strength > threshold)
            {
                frame_buffer.setPixel(x, y, outline_color);
            }

        }
    }   
}