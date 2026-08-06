#pragma once

#include <cassert>

#include "Vec3.h"

namespace tinymath
{
    template <typename T>
    struct Vec4
    {
        T x;
        T y;
        T z;
        T w;

        // operators
        
        Vec4<T> operator +(const Vec4<T>& b) const
        {
            return Vec4<T>{x + b.x, y + b.y, z + b.z, w + b.w};
        }

        Vec4<T> operator -(const Vec4<T>& b) const
        {
            return Vec4<T>{x - b.x, y - b.y, z - b.z, w - b.w};
        }

        Vec4<T> operator *(const T b) const
        {
            return Vec4<T>{x * b, y * b, z * b, w * b};
        }
    };

    
    using Vec4f = Vec4<float>; 


    inline Vec4f toVec4(Vec3f vec3)
    {
        return {vec3.x, vec3.y, vec3.z, 1.0f};
    } 

    /*
        Converts a Vec4f to a Vec3f with perspective divide, w == 0 can exists
        as a point at infinity (a direction only). so it is guarded by the assert.
    */
    inline Vec3f toVec3(Vec4f vec4)
    {
        assert(vec4.w != 0.0f); 
        return {vec4.x / vec4.w, vec4.y / vec4.w, vec4.z / vec4.w};
    }


} // end of tinymath name space