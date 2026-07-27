#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "rasterizer/TriangleRasterizer.h"
#include "rasterizer/primitives/Triangle.h"
#include "rasterizer/Framebuffer.h"
#include "rasterizer/Color.h"
#include "math/Vec2.h"


namespace
{
    const Color WHITE{255, 255, 255, 255};

    // Right triangle on a 16x16 buffer: right angle at (2,2), legs along the
    // axes, hypotenuse on the line x + y = 16. Interior is x >= 2, y >= 2,
    // x + y <= 16 — easy to reason about which pixels are in/out/on an edge.
    const Triangle TRI{tinymath::Vec2i{2, 2}, tinymath::Vec2i{14, 2}, tinymath::Vec2i{2, 14}};

    // Strictly inside — no pixel sits on an edge, so both rungs must fill them.
    const std::vector<tinymath::Vec2i> INTERIOR = {
        {3, 3}, {4, 4}, {5, 5}, {7, 4}, {4, 7},
    };

    // Clearly outside: beyond the hypotenuse, or past a leg.
    const std::vector<tinymath::Vec2i> EXTERIOR = {
        {10, 10}, {12, 12}, {15, 15}, {1, 10}, {10, 1},
    };

    using DrawTriangleFn = void (*)(Triangle, Color, Framebuffer&);

    struct Rung
    {
        const char* name;
        DrawTriangleFn fn;
    };

    const Rung RUNGS[] = {
        {"scanline", &TriangleRasterizer::drawTriangleScanline},
        {"barycentric", &TriangleRasterizer::drawTriangle},
    };

    // A pixel counts as "set" if any channel is non-zero (the framebuffer clears to all-zero).
    bool isSet(const Framebuffer& fb, int x, int y)
    {
        const uint8_t* pixel = fb.getData() + (y * fb.getWidth() + x) * sizeof(Color);
        return pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0 || pixel[3] != 0;
    }

    int countSetPixels(const Framebuffer& fb)
    {
        int count = 0;
        for (int y = 0; y < fb.getHeight(); ++y)
        {
            for (int x = 0; x < fb.getWidth(); ++x)
            {
                if (isSet(fb, x, y))
                {
                    ++count;
                }
            }
        }
        return count;
    }
}


// The core contract, checked per rung independently (we ship only one rung, so
// we do NOT require the two to be bit-identical — their edge conventions differ).
TEST_CASE("both rungs fill the interior and leave the exterior clear", "[triangle]")
{
    for (const Rung& rung : RUNGS)
    {
        Framebuffer fb(16, 16);
        rung.fn(TRI, WHITE, fb);

        for (const tinymath::Vec2i& p : INTERIOR)
        {
            INFO(rung.name << " interior (" << p.x << ", " << p.y << ")");
            REQUIRE(isSet(fb, p.x, p.y));
        }
        for (const tinymath::Vec2i& p : EXTERIOR)
        {
            INFO(rung.name << " exterior (" << p.x << ", " << p.y << ")");
            REQUIRE_FALSE(isSet(fb, p.x, p.y));
        }
    }
}


// The >= 0 boundary convention is exact for the shipped rung (integer edge
// functions), so we pin it there: a pixel sitting exactly on an edge is drawn.
TEST_CASE("drawTriangle includes edge pixels (>= 0 convention)", "[triangle]")
{
    Framebuffer fb(16, 16);
    TriangleRasterizer::drawTriangle(TRI, WHITE, fb);

    REQUIRE(isSet(fb, 8, 2)); // on leg AB (bottom edge, y = 2)
    REQUIRE(isSet(fb, 2, 8)); // on leg AC (left edge, x = 2)
    REQUIRE(isSet(fb, 8, 8)); // on hypotenuse BC (x + y = 16)
}


TEST_CASE("both rungs draw nothing for a degenerate (collinear) triangle", "[triangle]")
{
    const Triangle degenerate{tinymath::Vec2i{2, 2}, tinymath::Vec2i{6, 6}, tinymath::Vec2i{10, 10}};

    for (const Rung& rung : RUNGS)
    {
        Framebuffer fb(16, 16);
        rung.fn(degenerate, WHITE, fb);

        INFO(rung.name << " degenerate triangle");
        REQUIRE(countSetPixels(fb) == 0);
    }
}
