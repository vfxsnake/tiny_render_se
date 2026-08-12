#pragma once

#include <cmath>

namespace tinymath
{
    template <typename T>
    struct Vec3
    {
        T x;
        T y;
        T z;

        // operators
        
        Vec3<T> operator +(const Vec3<T>& b) const
        {
            return Vec3<T>{x + b.x, y + b.y, z + b.z};
        }

        Vec3<T> operator -(const Vec3<T>& b) const
        {
            return Vec3<T>{x - b.x, y - b.y, z - b.z};
        }

        Vec3<T> operator *(const T b) const
        {
            return Vec3<T>{x * b, y * b, z * b};
        }
    };


    template <typename T>
    T dot(Vec3<T> a, Vec3<T> b)
    {
        return (a.x * b.x) + (a.y * b.y) + (a.z * b.z); 
    }


    template <typename T>
    Vec3<T> cross(Vec3<T> a, Vec3<T>b)
    {
        return Vec3<T>{
            a.y * b.z - b.y * a.z,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x 
        };
    }


    template <typename T>
    T length(Vec3<T> vec)
    {
        return std::sqrt(dot(vec, vec));
    }


    template <typename T>
    Vec3<T> normalize(Vec3<T> vec)
    {
        // unguarded version of normalize, this is on purpose to demonstrate a misbehavior.
        T vec_length = length(vec);
        return vec * (1 / vec_length);
    }


    using Vec3f = Vec3<float>; 

} // end of tinymath name space