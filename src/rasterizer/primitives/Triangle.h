#pragma once

#include <utility>
#include <algorithm>
#include <array>

#include "math/Vec2.h"


struct Triangle
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