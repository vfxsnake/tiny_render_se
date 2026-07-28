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