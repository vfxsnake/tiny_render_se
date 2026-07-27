#pragma once

namespace tinymath
{
    template <typename T>
    struct Vec2
    {
        T x;
        T y;

        Vec2<T> operator -(const Vec2<T>& b) const
        {
            return Vec2<T>{x - b.x, y - b.y}; 
        } 
    };


    template <typename T>
    T cross(Vec2<T> a, Vec2<T>b)
    {
        return (a.x * b.y) - (a.y * b.x);
    }

    using Vec2i = Vec2<int>;

} // end of tinymath name space