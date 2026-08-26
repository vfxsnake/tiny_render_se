#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <vector>

#include "rasterizer/TriangleRasterizer.h"
#include "rasterizer/primitives/Triangle.h"
#include "rasterizer/Framebuffer.h"
#include "rasterizer/Color.h"
#include "rasterizer/ScreenSpace.h"
#include "math/Vec2.h"


namespace
{
    const Color WHITE{255, 255, 255, 255};

    // Right triangle on a 16x16 buffer: right angle at (2,2), legs along the
    // axes, hypotenuse on the line x + y = 16. Interior is x >= 2, y >= 2,
    // x + y <= 16 — easy to reason about which pixels are in/out/on an edge.
    const Triangle2D TRI{tinymath::Vec2i{2, 2}, tinymath::Vec2i{14, 2}, tinymath::Vec2i{2, 14}};

    // Strictly inside — no pixel sits on an edge, so both rungs must fill them.
    const std::vector<tinymath::Vec2i> INTERIOR = {
        {3, 3}, {4, 4}, {5, 5}, {7, 4}, {4, 7},
    };

    // Clearly outside: beyond the hypotenuse, or past a leg.
    const std::vector<tinymath::Vec2i> EXTERIOR = {
        {10, 10}, {12, 12}, {15, 15}, {1, 10}, {10, 1},
    };

    using DrawTriangleFn = void (*)(const Triangle2D&, Color, Framebuffer&);

    struct Rung
    {
        const char* name;
        DrawTriangleFn fn;
    };

    const Rung RUNGS[] = {
        {"scanline", &TriangleRasterizer::drawTriangleScanline},
        {"barycentric", &TriangleRasterizer::drawTriangle2D},
    };

    // A pixel counts as "set" if any channel is non-zero (the framebuffer clears to all-zero).
    bool isSet(const Framebuffer& fb, int x, int y)
    {
        Color pixel = fb.getPixel(x, y);
        return pixel.r != 0 || pixel.g != 0 || pixel.b != 0 || pixel.a != 0;
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
    TriangleRasterizer::drawTriangle2D(TRI, WHITE, fb);

    REQUIRE(isSet(fb, 8, 2)); // on leg AB (bottom edge, y = 2)
    REQUIRE(isSet(fb, 2, 8)); // on leg AC (left edge, x = 2)
    REQUIRE(isSet(fb, 8, 8)); // on hypotenuse BC (x + y = 16)
}


TEST_CASE("both rungs draw nothing for a degenerate (collinear) triangle", "[triangle]")
{
    const Triangle2D degenerate{tinymath::Vec2i{2, 2}, tinymath::Vec2i{6, 6}, tinymath::Vec2i{10, 10}};

    for (const Rung& rung : RUNGS)
    {
        Framebuffer fb(16, 16);
        rung.fn(degenerate, WHITE, fb);

        INFO(rung.name << " degenerate triangle");
        REQUIRE(countSetPixels(fb) == 0);
    }
}


// ---------------------------------------------------------------------------
// Lesson 3 — screen-space layer (RasterVertex / Triangle) and the depth test.
// ---------------------------------------------------------------------------

namespace
{
    const Color RED{255, 0, 0, 255};
    const Color BLUE{0, 0, 255, 255};
    const Color BLACK{0, 0, 0, 0};

    // Same shape as TRI, now in screen space: right angle at (2,2), legs of
    // length 12 along the axes, hypotenuse on x + y = 16. Wound counter-
    // clockwise in the y-up framebuffer, so it is front-facing and its
    // twiceSignedArea is +144 (the geometric area is 72).
    const float FRONT_TWICE_AREA = 144.0f;

    Triangle frontFacing(float depth_a, float depth_b, float depth_c)
    {
        return Triangle{
            {{2.0f, 2.0f}, depth_a},
            {{14.0f, 2.0f}, depth_b},
            {{2.0f, 14.0f}, depth_c}
        };
    }

    // The same three points with b and c exchanged: identical coverage on
    // screen, opposite winding, so the signed area flips sign.
    Triangle backFacing(float depth_a, float depth_b, float depth_c)
    {
        return Triangle{
            {{2.0f, 2.0f}, depth_a},
            {{2.0f, 14.0f}, depth_b},
            {{14.0f, 2.0f}, depth_c}
        };
    }

    // Strictly inside the triangle above — no point sits on an edge.
    const tinymath::Vec2f INSIDE{5.0f, 5.0f};

    bool sameImage(const Framebuffer& lhs, const Framebuffer& rhs)
    {
        if (lhs.getWidth() != rhs.getWidth() || lhs.getHeight() != rhs.getHeight())
        {
            return false;
        }
        for (int y = 0; y < lhs.getHeight(); ++y)
        {
            for (int x = 0; x < lhs.getWidth(); ++x)
            {
                if (!(lhs.getPixel(x, y) == rhs.getPixel(x, y)))
                {
                    return false;
                }
            }
        }
        return true;
    }
}


TEST_CASE("twiceSignedArea is twice the geometric area and carries the winding sign", "[screen]")
{
    REQUIRE(screen::twiceSignedArea(frontFacing(0.5f, 0.5f, 0.5f)) == Catch::Approx(FRONT_TWICE_AREA));
    REQUIRE(screen::twiceSignedArea(backFacing(0.5f, 0.5f, 0.5f)) == Catch::Approx(-FRONT_TWICE_AREA));
}


TEST_CASE("twiceSignedArea returns 0 for collinear vertices", "[screen]")
{
    const Triangle collinear{
        {{2.0f, 2.0f}, 0.5f},
        {{6.0f, 6.0f}, 0.5f},
        {{10.0f, 10.0f}, 0.5f}
    };

    REQUIRE(screen::twiceSignedArea(collinear) == Catch::Approx(0.0f));
}


// Depth is an interpolated attribute, not a coordinate: moving a vertex along
// z must not change the triangle's area or its facing.
TEST_CASE("twiceSignedArea ignores depth", "[screen]")
{
    REQUIRE(screen::twiceSignedArea(frontFacing(0.0f, 1.0f, 0.25f)) ==
            Catch::Approx(screen::twiceSignedArea(frontFacing(0.9f, 0.9f, 0.9f))));
}


// The bounds are documented as raw floats — the caller owns rounding and
// clamping — so a triangle hanging off the framebuffer must come back with its
// out-of-range bounds intact.
TEST_CASE("boundingBox returns raw bounds, neither rounded nor clamped", "[screen]")
{
    const Triangle spilling{
        {{-3.5f, 2.25f}, 0.5f},
        {{20.75f, -1.5f}, 0.5f},
        {{5.0f, 9.5f}, 0.5f}
    };

    const screen::BBox bbox = screen::boundingBox(spilling);

    REQUIRE(bbox.x_min == Catch::Approx(-3.5f));
    REQUIRE(bbox.x_max == Catch::Approx(20.75f));
    REQUIRE(bbox.y_min == Catch::Approx(-1.5f));
    REQUIRE(bbox.y_max == Catch::Approx(9.5f));
}


// The pairing test: the weight of a vertex is the area of the sub-triangle
// opposite it, so at vertex a the weights must be exactly (1, 0, 0). A rotated
// pairing still sums to 1 and still reports coverage correctly — this is the
// only assertion that catches it.
TEST_CASE("barycentricWeights collapse to 1/0/0 at each vertex", "[screen]")
{
    const Triangle tri = frontFacing(0.5f, 0.5f, 0.5f);

    const screen::BarycentricWeights at_a =
        screen::barycentricWeights(tri, tri.a.coordinates, FRONT_TWICE_AREA);
    REQUIRE(at_a.alpha == Catch::Approx(1.0f));
    REQUIRE(at_a.beta == Catch::Approx(0.0f));
    REQUIRE(at_a.gamma == Catch::Approx(0.0f));

    const screen::BarycentricWeights at_b =
        screen::barycentricWeights(tri, tri.b.coordinates, FRONT_TWICE_AREA);
    REQUIRE(at_b.alpha == Catch::Approx(0.0f));
    REQUIRE(at_b.beta == Catch::Approx(1.0f));
    REQUIRE(at_b.gamma == Catch::Approx(0.0f));

    const screen::BarycentricWeights at_c =
        screen::barycentricWeights(tri, tri.c.coordinates, FRONT_TWICE_AREA);
    REQUIRE(at_c.alpha == Catch::Approx(0.0f));
    REQUIRE(at_c.beta == Catch::Approx(0.0f));
    REQUIRE(at_c.gamma == Catch::Approx(1.0f));
}


TEST_CASE("barycentricWeights sum to 1 inside and outside the triangle", "[screen]")
{
    const Triangle tri = frontFacing(0.5f, 0.5f, 0.5f);

    const std::vector<tinymath::Vec2f> points = {
        {5.0f, 5.0f},   // inside
        {6.0f, 6.0f},   // the centroid
        {13.0f, 13.0f}, // outside, past the hypotenuse
        {0.0f, 0.0f}    // outside, past both legs
    };

    for (const tinymath::Vec2f& p : points)
    {
        const screen::BarycentricWeights w = screen::barycentricWeights(tri, p, FRONT_TWICE_AREA);
        INFO("point (" << p.x << ", " << p.y << ")");
        REQUIRE(w.alpha + w.beta + w.gamma == Catch::Approx(1.0f));
    }
}


TEST_CASE("barycentricWeights are 1/3 each at the centroid", "[screen]")
{
    const Triangle tri = frontFacing(0.5f, 0.5f, 0.5f);
    const screen::BarycentricWeights w =
        screen::barycentricWeights(tri, tinymath::Vec2f{6.0f, 6.0f}, FRONT_TWICE_AREA);

    REQUIRE(w.alpha == Catch::Approx(1.0f / 3.0f));
    REQUIRE(w.beta == Catch::Approx(1.0f / 3.0f));
    REQUIRE(w.gamma == Catch::Approx(1.0f / 3.0f));
}


// The single >= 0 coverage test must hold for both windings: dividing by the
// signed area cancels the sign the raw sub-areas carry.
TEST_CASE("the all-weights >= 0 coverage test holds for both windings", "[screen]")
{
    const Triangle front = frontFacing(0.5f, 0.5f, 0.5f);
    const Triangle back = backFacing(0.5f, 0.5f, 0.5f);

    const screen::BarycentricWeights inside_front =
        screen::barycentricWeights(front, INSIDE, FRONT_TWICE_AREA);
    const screen::BarycentricWeights inside_back =
        screen::barycentricWeights(back, INSIDE, -FRONT_TWICE_AREA);

    REQUIRE((inside_front.alpha >= 0.0f && inside_front.beta >= 0.0f && inside_front.gamma >= 0.0f));
    REQUIRE((inside_back.alpha >= 0.0f && inside_back.beta >= 0.0f && inside_back.gamma >= 0.0f));

    const tinymath::Vec2f outside{13.0f, 13.0f};
    const screen::BarycentricWeights outside_front =
        screen::barycentricWeights(front, outside, FRONT_TWICE_AREA);
    const screen::BarycentricWeights outside_back =
        screen::barycentricWeights(back, outside, -FRONT_TWICE_AREA);

    REQUIRE_FALSE((outside_front.alpha >= 0.0f && outside_front.beta >= 0.0f && outside_front.gamma >= 0.0f));
    REQUIRE_FALSE((outside_back.alpha >= 0.0f && outside_back.beta >= 0.0f && outside_back.gamma >= 0.0f));
}


TEST_CASE("drawTriangle draws nothing for a degenerate (collinear) triangle", "[triangle][depth]")
{
    const Triangle collinear{
        {{2.0f, 2.0f}, 0.5f},
        {{6.0f, 6.0f}, 0.5f},
        {{10.0f, 10.0f}, 0.5f}
    };

    Framebuffer fb(16, 16);
    TriangleRasterizer::drawTriangle(collinear, WHITE, fb, false);

    REQUIRE(countSetPixels(fb) == 0);
}


TEST_CASE("drawTriangle interpolates depth across the triangle", "[depth]")
{
    // Near at vertex a (depth 1.0 = near), far along the opposite edge.
    Framebuffer fb(16, 16);
    TriangleRasterizer::drawTriangle(frontFacing(1.0f, 0.0f, 0.0f), WHITE, fb);

    // At vertex a the weights are (1, 0, 0), so the depth is a's depth exactly.
    REQUIRE(fb.getDepth(2, 2) == Catch::Approx(1.0f));

    // The centroid takes a third of each: (1 + 0 + 0) / 3.
    REQUIRE(fb.getDepth(6, 6) == Catch::Approx(1.0f / 3.0f));

    // Depth falls off monotonically as the pixel moves away from a.
    REQUIRE(fb.getDepth(3, 3) > fb.getDepth(6, 6));
}


// The property the pre-z-buffer renderer fails: the winner is decided by depth,
// not by who was drawn last.
TEST_CASE("the nearer triangle wins regardless of draw order", "[depth]")
{
    const Triangle far_tri = frontFacing(0.25f, 0.25f, 0.25f);
    const Triangle near_tri = frontFacing(0.75f, 0.75f, 0.75f);

    SECTION("far drawn first, near second")
    {
        Framebuffer fb(16, 16);
        TriangleRasterizer::drawTriangle(far_tri, BLUE, fb);
        TriangleRasterizer::drawTriangle(near_tri, RED, fb);

        REQUIRE(fb.getPixel(5, 5) == RED);
        REQUIRE(fb.getDepth(5, 5) == Catch::Approx(0.75f));
    }

    SECTION("near drawn first, far second")
    {
        Framebuffer fb(16, 16);
        TriangleRasterizer::drawTriangle(near_tri, RED, fb);
        TriangleRasterizer::drawTriangle(far_tri, BLUE, fb);

        REQUIRE(fb.getPixel(5, 5) == RED);
        REQUIRE(fb.getDepth(5, 5) == Catch::Approx(0.75f));
    }
}


TEST_CASE("a fragment behind the z-buffer leaves both the colour and the depth alone", "[depth]")
{
    Framebuffer fb(16, 16);
    TriangleRasterizer::drawTriangle(frontFacing(0.75f, 0.75f, 0.75f), RED, fb);
    TriangleRasterizer::drawTriangle(frontFacing(0.25f, 0.25f, 0.25f), BLUE, fb);

    REQUIRE(fb.getPixel(5, 5) == RED);
    REQUIRE(fb.getDepth(5, 5) == Catch::Approx(0.75f));
}


TEST_CASE("back-face culling drops back-facing triangles and keeps front-facing ones", "[culling]")
{
    SECTION("a back-facing triangle is dropped when culling is on")
    {
        Framebuffer fb(16, 16);
        TriangleRasterizer::drawTriangle(backFacing(0.5f, 0.5f, 0.5f), WHITE, fb, true);
        REQUIRE(countSetPixels(fb) == 0);
    }

    SECTION("the same triangle is drawn when culling is off")
    {
        Framebuffer fb(16, 16);
        TriangleRasterizer::drawTriangle(backFacing(0.5f, 0.5f, 0.5f), WHITE, fb, false);
        REQUIRE(countSetPixels(fb) > 0);
    }

    SECTION("a front-facing triangle survives culling")
    {
        Framebuffer fb(16, 16);
        TriangleRasterizer::drawTriangle(frontFacing(0.5f, 0.5f, 0.5f), WHITE, fb, true);
        REQUIRE(countSetPixels(fb) > 0);
    }
}


// With a working z-buffer, culling is a pure optimization: for front-facing
// geometry it may change the time taken but never a single pixel.
TEST_CASE("culling does not change the image of front-facing geometry", "[culling]")
{
    Framebuffer culled(16, 16);
    Framebuffer unculled(16, 16);
    culled.clear(BLACK);
    unculled.clear(BLACK);

    const Triangle far_tri = frontFacing(0.25f, 0.25f, 0.25f);
    const Triangle near_tri = frontFacing(0.75f, 0.75f, 0.75f);

    TriangleRasterizer::drawTriangle(far_tri, BLUE, culled, true);
    TriangleRasterizer::drawTriangle(near_tri, RED, culled, true);

    TriangleRasterizer::drawTriangle(far_tri, BLUE, unculled, false);
    TriangleRasterizer::drawTriangle(near_tri, RED, unculled, false);

    REQUIRE(sameImage(culled, unculled));
}
