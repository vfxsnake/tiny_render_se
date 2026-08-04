#include "Transform.h"

#include <cmath>


tinymath::Vec3f tinymath::rotateY(tinymath::Vec3f point_position, float angle_in_radians)
{
    float cos_theta = std::cos(angle_in_radians);
    float sin_theta = std::sin(angle_in_radians);

    float x_rotated = point_position.x * cos_theta + point_position.z * sin_theta;
    float z_rotated = point_position.z * cos_theta - point_position.x * sin_theta;

    return {x_rotated, point_position.y, z_rotated};
}