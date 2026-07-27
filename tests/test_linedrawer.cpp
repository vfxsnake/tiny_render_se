#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <vector>

#include "rasterizer/LineDrawer.h"
#include "rasterizer/Framebuffer.h"
#include "rasterizer/Color.h"
#include "math/Vec2.h"


namespace
{
    const Color WHITE{255, 255, 255, 255};

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

    // Byte-for-byte comparison of two same-sized framebuffers.
    bool identical(const Framebuffer& a, const Framebuffer& b)
    {
        const int total_bytes = a.getWidth() * a.getHeight() * static_cast<int>(sizeof(Color));
        return std::memcmp(a.getData(), b.getData(), total_bytes) == 0;
    }
}


TEST_CASE("drawLine horizontal line is gap-free and endpoint-inclusive", "[linedrawer]")
{
    Framebuffer fb(16, 16);
    LineDrawer::drawLine({2, 8}, {12, 8}, WHITE, fb);

    for (int x = 2; x <= 12; ++x)
    {
        REQUIRE(isSet(fb, x, 8));
    }
    REQUIRE(countSetPixels(fb) == 11); // x = 2..12 inclusive, nothing else
}


TEST_CASE("drawLine vertical line is gap-free and endpoint-inclusive", "[linedrawer]")
{
    Framebuffer fb(16, 16);
    LineDrawer::drawLine({6, 2}, {6, 13}, WHITE, fb);

    for (int y = 2; y <= 13; ++y)
    {
        REQUIRE(isSet(fb, 6, y));
    }
    REQUIRE(countSetPixels(fb) == 12); // y = 2..13 inclusive
}


TEST_CASE("drawLine 45-degree diagonal sets the whole diagonal", "[linedrawer]")
{
    Framebuffer fb(16, 16);
    LineDrawer::drawLine({0, 0}, {10, 10}, WHITE, fb);

    for (int i = 0; i <= 10; ++i)
    {
        REQUIRE(isSet(fb, i, i));
    }
    REQUIRE(countSetPixels(fb) == 11);
}


TEST_CASE("drawLine steep line has no gaps on the major (y) axis", "[linedrawer]")
{
    Framebuffer fb(16, 32);
    LineDrawer::drawLine({4, 2}, {8, 28}, WHITE, fb); // |dy| = 26 > |dx| = 4 => steep

    // Every row in the span must contain at least one pixel (no gaps along the lead axis).
    for (int y = 2; y <= 28; ++y)
    {
        bool row_has_pixel = false;
        for (int x = 0; x < fb.getWidth() && !row_has_pixel; ++x)
        {
            row_has_pixel = isSet(fb, x, y);
        }
        INFO("row y = " << y);
        REQUIRE(row_has_pixel);
    }
}


TEST_CASE("drawLine sets both endpoints", "[linedrawer]")
{
    Framebuffer fb(32, 32);
    tinymath::Vec2i a{3, 5};
    tinymath::Vec2i b{20, 14};
    LineDrawer::drawLine(a, b, WHITE, fb);

    REQUIRE(isSet(fb, a.x, a.y));
    REQUIRE(isSet(fb, b.x, b.y));
}


TEST_CASE("drawLine is direction-agnostic: draw(a,b) == draw(b,a)", "[linedrawer]")
{
    tinymath::Vec2i a{3, 5};
    tinymath::Vec2i b{20, 14};

    Framebuffer forward(32, 32);
    Framebuffer backward(32, 32);
    LineDrawer::drawLine(a, b, WHITE, forward);
    LineDrawer::drawLine(b, a, WHITE, backward);

    REQUIRE(identical(forward, backward));
}


TEST_CASE("drawLine with zero-length input (a == b) draws nothing", "[linedrawer]")
{
    Framebuffer fb(16, 16);
    LineDrawer::drawLine({8, 8}, {8, 8}, WHITE, fb);

    REQUIRE(countSetPixels(fb) == 0);
}


// Highest-value case: the three rungs must be bit-identical. Neither the on-screen
// differential test nor a shape test can prove this for every octant at once.
TEST_CASE("naive, accum and Bresenham light identical pixels across all octants", "[linedrawer]")
{
    const tinymath::Vec2i center{16, 16};
    const std::vector<tinymath::Vec2i> endpoints = {
        {28, 16}, // horizontal +x
        { 4, 16}, // horizontal -x
        {16, 28}, // vertical   +y
        {16,  4}, // vertical   -y
        {28, 24}, // shallow    +x +y
        { 4, 24}, // shallow    -x +y
        {28,  8}, // shallow    +x -y
        { 4,  8}, // shallow    -x -y
        {24, 28}, // steep      +x +y
        { 8, 28}, // steep      -x +y
        {24,  4}, // steep      +x -y
        { 8,  4}, // steep      -x -y
        {28, 28}, // 45         +x +y
        { 4, 28}, // 45         -x +y
        {28,  4}, // 45         +x -y
        { 4,  4}, // 45         -x -y
    };

    for (const tinymath::Vec2i& end : endpoints)
    {
        Framebuffer naive(32, 32);
        Framebuffer accum(32, 32);
        Framebuffer bresenham(32, 32);

        LineDrawer::drawLineNaive(center, end, WHITE, naive);
        LineDrawer::drawLineAccum(center, end, WHITE, accum);
        LineDrawer::drawLine(center, end, WHITE, bresenham);

        INFO("endpoint (" << end.x << ", " << end.y << ")");
        REQUIRE(identical(naive, accum));
        REQUIRE(identical(accum, bresenham));
    }
}
