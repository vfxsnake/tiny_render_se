#pragma once

#include <utility>
#include <algorithm>
#include <array>

#include "math/Vec2.h"


/*
    RasterVertex represents a point in screen space with subpixel precision (Vec2f) and 
    depth information (float) with a range from [0, 1] where 0 means far and 1 means near.
    Required for the Rasterizer to perform operation conveniently.
*/
struct RasterVertex
{
    tinymath::Vec2f coordinates;
    float depth;
};

/*
    A rasterize primitive that holds 3 RasterVertex to represent a triangle
    in screen or projected space.
*/
struct Triangle
{
    RasterVertex a; 
    RasterVertex b; 
    RasterVertex c;
};


/*
    2d triangle first implemented for the rasterize algorithms, it is currently deprecated.
    no more development will be added to it.
    Uses a Vec2<int>.
    have some helper functions to accomplish the rasterize algorithms:
        sortedByY() sort point in y ascending order,
        getXBounds and getYBounds returns pairs of min and max values in the corresponding axis.
*/
struct Triangle2D
{
    tinymath::Vec2i a;
    tinymath::Vec2i b;
    tinymath::Vec2i c;

    /*
        Function for showing the triangle data sorted by 
        the component "y" it does not alter the triangle data order.
    */
    std::array<tinymath::Vec2i, 3> sortedByY() const
    {
        std::array<tinymath::Vec2i, 3> vertices{a, b, c};

        // swap by comparison
        if (vertices[0].y > vertices[1].y)
        {
            std::swap(vertices[0], vertices[1]);
        }
        
        if (vertices[1].y > vertices[2].y)
        {
            std::swap(vertices[1], vertices[2]);
        }

        if (vertices[0].y > vertices[1].y)
        {
            std::swap(vertices[0], vertices[1]);
        }

        return vertices;
    }

    std::pair<int, int> getXBounds() const
    {
        return {
            std::min({a.x, b.x, c.x}),
            std::max({a.x, b.x, c.x})
        };
    }

    std::pair<int, int> getYBounds() const
    {
        return {
            std::min({a.y, b.y, c.y}),
            std::max({a.y, b.y, c.y})
        };
    }
};
