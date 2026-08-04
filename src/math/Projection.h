#pragma once

#include "Vec3.h"

namespace tinymath
{
    Vec3f orthographicProjection(
        Vec3f point_position, 
        int screen_width, 
        int screen_height, 
        float depth_scale
    );


    Vec3f perspectiveZDivide(Vec3f point_position, float eye_distance);
    
} // end of tinymath namespace