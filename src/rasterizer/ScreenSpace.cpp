#include "ScreenSpace.h"

#include <algorithm>

float screen::twiceSignedArea(const Triangle& triangle)
{
    tinymath::Vec2f segment_a_b = triangle.b.coordinates - triangle.a.coordinates;
    tinymath::Vec2f segment_a_c = triangle.c.coordinates - triangle.a.coordinates;

    return tinymath::cross(segment_a_b, segment_a_c);
}


screen::BBox screen::boundingBox(const Triangle& triangle)
{
    screen::BBox b_box;

    b_box.x_min = std::min({triangle.a.coordinates.x, triangle.b.coordinates.x, triangle.c.coordinates.x});
    b_box.x_max = std::max({triangle.a.coordinates.x, triangle.b.coordinates.x, triangle.c.coordinates.x});
    b_box.y_min = std::min({triangle.a.coordinates.y, triangle.b.coordinates.y, triangle.c.coordinates.y});
    b_box.y_max = std::max({triangle.a.coordinates.y, triangle.b.coordinates.y, triangle.c.coordinates.y});

    return b_box;
}


screen::BarycentricWeights screen::barycentricWeights(
    const Triangle& triangle, 
    tinymath::Vec2f raster_point, 
    float twice_signed_area
)
{
    float alpha_area = tinymath::cross(
        triangle.c.coordinates - triangle.b.coordinates, 
        raster_point - triangle.b.coordinates
    );

    float beta_area = tinymath::cross(
        triangle.a.coordinates - triangle.c.coordinates, 
        raster_point - triangle.c.coordinates
    );
    
    float gamma_area = tinymath::cross(
        triangle.b.coordinates - triangle.a.coordinates, 
        raster_point - triangle.a.coordinates
    );

    screen::BarycentricWeights weights;
    weights.alpha = alpha_area / twice_signed_area;
    weights.beta = beta_area / twice_signed_area;
    weights.gamma = gamma_area / twice_signed_area;

    return weights;
}
