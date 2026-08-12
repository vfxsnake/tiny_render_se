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


tinymath::Matrix4x4 tinymath::lookAt(
    Vec3f eye_position, 
    Vec3f target_position, 
    Vec3f up_vector
)
{
    Vec3f center_to_eye_direction = normalize(
        eye_position - target_position // back ward direction. from center to camera.
    );  
    
    Vec3f right_vector = normalize(
        cross(up_vector, center_to_eye_direction)
    );
    
    Vec3f true_up_vector = normalize(
        cross(center_to_eye_direction, right_vector)
    );

    Matrix4x4 rotation_matrix;
    
    rotation_matrix.data[0][0] = right_vector.x;
    rotation_matrix.data[0][1] = right_vector.y;
    rotation_matrix.data[0][2] = right_vector.z;

    rotation_matrix.data[1][0] = true_up_vector.x;
    rotation_matrix.data[1][1] = true_up_vector.y;
    rotation_matrix.data[1][2] = true_up_vector.z;

    rotation_matrix.data[2][0] = center_to_eye_direction.x;
    rotation_matrix.data[2][1] = center_to_eye_direction.y;
    rotation_matrix.data[2][2] = center_to_eye_direction.z;

    Matrix4x4 offset_matrix;

    offset_matrix.data[0][3] = -target_position.x;
    offset_matrix.data[1][3] = -target_position.y;
    offset_matrix.data[2][3] = -target_position.z;

    return rotation_matrix * offset_matrix;
}