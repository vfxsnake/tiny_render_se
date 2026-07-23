#include "TriangleRasterizer.h"

#include <array>
#include <utility>

#include "Framebuffer.h"



void TriangleRasterizer::drawTriangleScanline(
    Triangle triangle,
    Color color,
    Framebuffer& frame_buffer
)
{
    std::array<Vec2i, 3> vertices = triangle.sortedByY();
    // auto [min_x_bound, max_x_bound] = triangle.getXBounds();

    // long and mid segments, first half, runs along the large segment until the end of the middle.
    for (int y = vertices[0].y; y < vertices[1].y; y++)
    {
        if (y < 0 || y >= frame_buffer.getHeight())
        {
            // y is out of screen space 
            continue;
        }
        
        float y_long_t = (y - vertices[0].y) / float(vertices[2].y - vertices[0].y);
        float y_short_t = (y - vertices[0].y) / float(vertices[1].y - vertices[0].y);
        
        int x_bound_0 = vertices[0].x + (vertices[2].x - vertices[0].x) * y_long_t;
        int x_bound_1 = vertices[0].x + (vertices[1].x - vertices[0].x) * y_short_t;
        
        if (x_bound_0 > x_bound_1)
        {
            std::swap(x_bound_0, x_bound_1);
        }

        for (int x = x_bound_0; x <= x_bound_1; x++)
        {
            
            if (x < 0 || x >= frame_buffer.getWidth())
            {
                // x is out of screen space
                continue;
            }
            
            frame_buffer.setPixel(x, y, color);
        }
    }

    // long and middle to last segments, second half, runs from middle point to the end of long segment.
    for (int y = vertices[1].y; y <= vertices[2].y; y++)
    {
        if (y < 0 || y >= frame_buffer.getHeight())
        {
            // y is out of screen space 
            continue;
        }
        
        float y_long_t = (y - vertices[0].y) / float(vertices[2].y - vertices[0].y);
        int x_bound_0 = vertices[0].x + (vertices[2].x - vertices[0].x) * y_long_t;
        
        int x_bound_1;
        // check for flat bottom vertices[2].y == vertices[1].y
        int delta_y = vertices[2].y - vertices[1].y;
    
        if (delta_y == 0)
        {
            x_bound_1 = vertices[1].x;
        }
        else
        {
            x_bound_1 = vertices[1].x + (vertices[2].x - vertices[1].x) * (y - vertices[1].y) /  float(delta_y);
        }
        
        
        if (x_bound_0 > x_bound_1)
        {
            std::swap(x_bound_0, x_bound_1);
        }

        for (int x = x_bound_0; x <= x_bound_1; x++)
        {
            
            if (x < 0 || x >= frame_buffer.getWidth())
            {
                // x is out of screen space
                continue;
            }
            
            frame_buffer.setPixel(x, y, color);
        }
    }
}
