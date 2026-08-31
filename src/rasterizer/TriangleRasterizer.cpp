#include "TriangleRasterizer.h"

#include <array>
#include <utility>
#include <algorithm>

#include "Framebuffer.h"
#include "ScreenSpace.h"
#include "shaders/AbstractShader.h"
#include "math/Projection.h"
#include "math/Matrix4x4.h"


void TriangleRasterizer::drawTriangleScanline(
    const Triangle2D& triangle,
    Color color,
    Framebuffer& frame_buffer
)
{
    if (cross(triangle.b - triangle.a, triangle.c - triangle.a) == 0)
    {
        return;
    }

    std::array<tinymath::Vec2i, 3> vertices = triangle.sortedByY();
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


void TriangleRasterizer::drawTriangle2D(
    const Triangle2D& triangle,
    Color color,
    Framebuffer& frame_buffer
)
{
    if (cross(triangle.b - triangle.a, triangle.c - triangle.a) == 0)
    {
        return;
    }
    // bounding box extraction
    auto [bbox_x_min, bbox_x_max] = triangle.getXBounds(); 
    auto [bbox_y_min, bbox_y_max] = triangle.getYBounds();

    for (int y = bbox_y_min; y <= bbox_y_max; y++)
    {
        for (int x = bbox_x_min; x <= bbox_x_max; x++)
        {
            tinymath::Vec2i current_position{x, y};
            int weight_0 = cross(triangle.c - triangle.b, current_position - triangle.b);
            int weight_1 = cross(triangle.a - triangle.c, current_position - triangle.c);
            int weight_2 = cross(triangle.b - triangle.a, current_position - triangle.a);

            bool all_positive = (weight_0 >= 0 && weight_1 >= 0 && weight_2 >= 0);
            bool all_negative = (weight_0 <= 0 && weight_1 <= 0 && weight_2 <= 0);
            
            if (all_positive || all_negative)
            {
                frame_buffer.setPixel(x, y, color);
            }
        }
    }
}


void TriangleRasterizer::drawTriangle(const Triangle& triangle, Color color, Framebuffer& frame_buffer, bool cull_back_faces)
{
    float parallelogram_area = screen::twiceSignedArea(triangle);
    
    // prevents degenerated faces
    if (parallelogram_area == 0.0f)
    {
        return;
    }

    if (cull_back_faces && parallelogram_area < 0.0f)
    {
        return;
    }


    screen::BBox bbox = screen::boundingBox(triangle);
    
    // clamping bbox to screen window boundaries. 
    int x_min = static_cast<int>(std::max(0.0f, bbox.x_min));
    int x_max = static_cast<int>(std::min(bbox.x_max, static_cast<float>(frame_buffer.getWidth() - 1)));
    int y_min = static_cast<int>(std::max(0.0f, bbox.y_min));
    int y_max = static_cast<int>(std::min(bbox.y_max, static_cast<float>(frame_buffer.getHeight() - 1)));
    

    for (int y = y_min; y <= y_max; y++)
    {
        for (int x = x_min; x <= x_max; x++)
        {
            tinymath::Vec2f current_position(static_cast<float>(x), static_cast<float>(y));
            screen::BarycentricWeights weights = screen::barycentricWeights(
                triangle, 
                current_position, 
                parallelogram_area
            );
            
            if (weights.alpha >= 0 && weights.beta >= 0 && weights.gamma >= 0)
            {
                // depth test
                float depth = triangle.a.depth * weights.alpha + 
                              triangle.b.depth * weights.beta + 
                              triangle.c.depth * weights.gamma;

                if (frame_buffer.getDepth(x, y) < depth)
                {
                    frame_buffer.setPixel(x, y, color);
                    frame_buffer.setDepth(x, y, depth);
                }
            }
        }
    }
}


void TriangleRasterizer::drawTriangleWithShader(
    const std::array<tinymath::Vec4f, 3>& clip_positions,
    AbstractShader& shader,
    Framebuffer& frame_buffer,
    bool cull_back_faces
)
{
    tinymath::Vec3f a_ndc = tinymath::toVec3(clip_positions[0]);
    tinymath::Vec3f b_ndc = tinymath::toVec3(clip_positions[1]);
    tinymath::Vec3f c_ndc = tinymath::toVec3(clip_positions[2]);
    
    tinymath::Matrix4x4 viewport_matrix = tinymath::viewport(frame_buffer.getWidth(), frame_buffer.getHeight());
    tinymath::Vec4f a_screen = viewport_matrix * tinymath::toVec4(a_ndc);
    tinymath::Vec4f b_screen = viewport_matrix * tinymath::toVec4(b_ndc);
    tinymath::Vec4f c_screen = viewport_matrix * tinymath::toVec4(c_ndc);
    
    Triangle triangle{
        {{a_screen.x, a_screen.y}, a_screen.z}, 
        {{b_screen.x, b_screen.y}, b_screen.z}, 
        {{c_screen.x, c_screen.y}, c_screen.z}
    };
    
    float parallelogram_area = screen::twiceSignedArea(triangle);
    
    // prevents degenerated faces
    if (parallelogram_area == 0.0f)
    {
        return;
    }

    if (cull_back_faces && parallelogram_area < 0.0f)
    {
        return;
    }


    screen::BBox bbox = screen::boundingBox(triangle);
    
    // clamping bbox to screen window boundaries. 
    int x_min = static_cast<int>(std::max(0.0f, bbox.x_min));
    int x_max = static_cast<int>(std::min(bbox.x_max, static_cast<float>(frame_buffer.getWidth() - 1)));
    int y_min = static_cast<int>(std::max(0.0f, bbox.y_min));
    int y_max = static_cast<int>(std::min(bbox.y_max, static_cast<float>(frame_buffer.getHeight() - 1)));
    

    for (int y = y_min; y <= y_max; y++)
    {
        for (int x = x_min; x <= x_max; x++)
        {
            tinymath::Vec2f current_position(static_cast<float>(x), static_cast<float>(y));
            screen::BarycentricWeights weights = screen::barycentricWeights(
                triangle, 
                current_position, 
                parallelogram_area
            );
            
            if (weights.alpha >= 0 && weights.beta >= 0 && weights.gamma >= 0)
            {
                // depth test
                float depth = triangle.a.depth * weights.alpha + 
                              triangle.b.depth * weights.beta + 
                              triangle.c.depth * weights.gamma;

                if (frame_buffer.getDepth(x, y) < depth)
                {
                    Color color;
                    if (!shader.fragment(weights, color))
                    {
                        continue;
                    }
                    frame_buffer.setPixel(x, y, color);
                    frame_buffer.setDepth(x, y, depth);
                    
                }
            }
        }
    }
}
