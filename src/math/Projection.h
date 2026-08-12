#pragma once

#include "Vec3.h"
#include "Matrix4x4.h"


namespace tinymath
{
    Vec3f orthographicProjection(
        Vec3f point_position, 
        int screen_width, 
        int screen_height, 
        float depth_scale
    );


    Vec3f perspectiveZDivide(Vec3f point_position, float eye_distance);

    Matrix4x4 viewport(int screen_width, int screen_height);

    Matrix4x4 perspective(float focal_length);
    
} // end of tinymath namespace