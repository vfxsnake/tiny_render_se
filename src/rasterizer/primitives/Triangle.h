#pragma once

#include <utility>
#include <array>

#include "math/Vec2.h"


struct Triangle
{
    Vec2i a;
    Vec2i b;
    Vec2i c;

    /*
        Function for showing the triangle data sorted by 
        the component "y" it does not alter the triangle data order.
    */
    std::array<Vec2i, 3> sortedByY() const
    {
        std::array<Vec2i, 3> vertices{a, b, c};

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

    std::pair<int, int> getXBounds()
    {
        return {
            std::min(std::min(a.x, b.x), c.x),
            std::max(std::max(a.x, b.x), c.x)
        };
    }
};