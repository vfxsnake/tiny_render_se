#include "Projection.h"


tinymath::Vec3f tinymath::orthographicProjection(
    Vec3f point_position,
    int screen_width,
    int screen_height,
    float depth_scale
)
{
    return {
        ((point_position.x + 1) * screen_width) / 2,
        ((point_position.y + 1) * screen_height) / 2,
        ((point_position.z + 1) * depth_scale) / 2,
    };
}


tinymath::Vec3f tinymath::perspectiveZDivide(tinymath::Vec3f point_position, float eye_distance)
{
    float k = 1.0f / (1.0f - (point_position.z / eye_distance));
    
    return point_position * k;
}


tinymath::Matrix4x4 tinymath::viewport(int screen_width, int screen_height)
{

    float half_width = screen_width * 0.5f;
    float half_height = screen_height * 0.5f;
    
    Matrix4x4 viewport_matrix; // identity matrix
    viewport_matrix.data[0][0] = half_width; // x scale factor
    viewport_matrix.data[0][3] = half_width; // x offset
    viewport_matrix.data[1][1] = half_height; // y scale factor
    viewport_matrix.data[1][3] = half_height; // y offset
    viewport_matrix.data[2][2] = 0.5f; // z scale factor
    viewport_matrix.data[2][3] = 0.5f; // z offset
    return viewport_matrix;
}


tinymath::Matrix4x4 tinymath::perspective(float focal_length)
{
    Matrix4x4 perspective_matrix;
    perspective_matrix.data[3][2] = -1 / focal_length;

    return perspective_matrix;
}
