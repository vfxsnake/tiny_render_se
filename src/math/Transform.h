#pragma once

#include "Vec3.h"
#include "Matrix4x4.h"

namespace tinymath
{
    
    Vec3f rotateY(Vec3f point_position, float angle_in_radians);

    Matrix4x4 lookAt(Vec3f eye_position, Vec3f target_position, Vec3f up_vector);

} // end of tinymath namespace