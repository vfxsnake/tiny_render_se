#pragma once

#include "primitives/Triangle.h"
#include "math/Vec2.h"

namespace screen
{
    struct BBox
    {
        float x_min;
        float y_min;
        float x_max;
        float y_max;
    };


    struct BarycentricWeights
    {
        float alpha;  // weight of vertex a
        float beta;   // weight of vertex b
        float gamma;  // weight of vertex c
    };

    /*
        Returns the signed area of the parallelogram (two times the area of the triangle) formed by
        the segments ab, ac of the triangle, the operation uses the Vec2f coordinates of the
        RasterVertex, the depth is ignored.

        The result is signed: one winding order gives a positive value and the opposite winding
        gives a negative one, so the sign identifies which way the triangle faces (the value
        back-face culling reads). The magnitude is twice the geometric area; the factor of two is
        never divided out because it cancels in barycentricWeights.

        A degenerate (collinear or zero-area) triangle returns 0.0f, which is the caller's signal
        not to call barycentricWeights, it divides by this value.
    */
    float twiceSignedArea(const Triangle& triangle);

    /*
        Returns a BBox representing the x,y min and x,y max of the projected triangle,
        computed from the Vec2f coordinates of the RasterVertex, the depth is ignored.

        The bounds are deliberately left as raw floats: they are neither rounded nor clamped to
        the framebuffer. Rounding is the caller's decision (floor the minimums, ceil the maximums,
        so no covered pixel is dropped) and so is clamping against the framebuffer width and
        height. Returning floats keeps that choice at the call site instead of baking a
        truncation rule into the primitive.
    */
    BBox boundingBox(const Triangle& triangle);


    /*
        Returns the barycentric weights of raster_point with respect to the triangle, using the
        Vec2f coordinates of the RasterVertex, the depth is ignored.

        Each weight belongs to the vertex it is named after: alpha weights a, beta weights b and
        gamma weights c. Any other pairing still sums to 1 and still reports coverage correctly,
        it only tilts the interpolated attributes, so it fails silently.

        The three weights sum to 1. raster_point lies inside the triangle when all three are
        >= 0, and that single test holds for both winding orders, dividing by the signed area
        cancels the winding sign that the raw weights carry.

        twice_signed_area is passed in rather than recomputed so that it is evaluated once per
        triangle instead of once per pixel.

        Preconditions: twice_signed_area must come from twiceSignedArea() called on this same
        triangle, and it must not be 0.0f, the weights are computed by dividing by it.
    */
    BarycentricWeights barycentricWeights(
        const Triangle& triangle, 
        tinymath::Vec2f raster_point, 
        float twice_signed_area
    );

} // end of screen namespace
