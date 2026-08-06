#include <catch2/catch_test_macros.hpp>

#include "math/Vec4.h"

TEST_CASE("Vec4 initialization and templating check", "[Vec4]")
{
    tinymath::Vec4<int> vec4_int{10, 20, 30, 1};
    REQUIRE(vec4_int.x == 10);
    REQUIRE(vec4_int.y == 20);
    REQUIRE(vec4_int.z == 30);
    REQUIRE(vec4_int.w == 1);

    // Values chosen to be exactly representable, so == is safe.
    tinymath::Vec4f vec4_float{0.5f, -1.75f, 2.25f, 1.0f};
    REQUIRE(vec4_float.x == 0.5f);
    REQUIRE(vec4_float.y == -1.75f);
    REQUIRE(vec4_float.z == 2.25f);
    REQUIRE(vec4_float.w == 1.0f);
}


TEST_CASE("Vec4 addition", "[Vec4]")
{
    tinymath::Vec4<int> a{10, 20, 30, 1};
    tinymath::Vec4<int> b{3, 8, -5, 1};

    // w is added like any other component. Two points with w == 1 sum to
    // w == 2, which after the divide is their midpoint.
    tinymath::Vec4<int> sum = a + b;
    REQUIRE(sum.x == 13);
    REQUIRE(sum.y == 28);
    REQUIRE(sum.z == 25);
    REQUIRE(sum.w == 2);
}


TEST_CASE("Vec4 subtraction", "[Vec4]")
{
    tinymath::Vec4<int> a{10, 20, 30, 1};
    tinymath::Vec4<int> b{3, 8, -5, 1};

    // Point minus point yields w == 0: a direction, which has no position.
    tinymath::Vec4<int> diff = a - b;
    REQUIRE(diff.x == 7);
    REQUIRE(diff.y == 12);
    REQUIRE(diff.z == 35);
    REQUIRE(diff.w == 0);
}


TEST_CASE("Vec4 scalar multiplication", "[Vec4]")
{
    tinymath::Vec4<int> a{2, -3, 4, 1};

    tinymath::Vec4<int> scaled = a * 5;
    REQUIRE(scaled.x == 10);
    REQUIRE(scaled.y == -15);
    REQUIRE(scaled.z == 20);
    REQUIRE(scaled.w == 5);

    // Callable on a const object (operator* is const).
    tinymath::Vec4f const b{0.5f, -1.5f, 2.0f, 1.0f};
    tinymath::Vec4f scaled_float = b * 2.0f;
    REQUIRE(scaled_float.x == 1.0f);
    REQUIRE(scaled_float.y == -3.0f);
    REQUIRE(scaled_float.z == 4.0f);
    REQUIRE(scaled_float.w == 2.0f);
}


TEST_CASE("toVec4 embeds a point with w = 1", "[Vec4]")
{
    tinymath::Vec3f const point{0.5f, -1.75f, 2.25f};

    tinymath::Vec4f embedded = toVec4(point);
    REQUIRE(embedded.x == 0.5f);
    REQUIRE(embedded.y == -1.75f);
    REQUIRE(embedded.z == 2.25f);
    REQUIRE(embedded.w == 1.0f);
}


TEST_CASE("toVec3 divides through by w", "[Vec4]")
{
    // w == 1 is the case that proves nothing: it passes whether the function
    // divides or just drops the component. w != 1 is what catches that.
    tinymath::Vec4f const homogeneous{3.0f, -6.0f, 12.0f, 4.0f};

    tinymath::Vec3f divided = toVec3(homogeneous);
    REQUIRE(divided.x == 0.75f);
    REQUIRE(divided.y == -1.5f);
    REQUIRE(divided.z == 3.0f);

    // A negative w flips every sign — the divide is signed, not a magnitude.
    tinymath::Vec3f negative_w = toVec3(tinymath::Vec4f{3.0f, -6.0f, 12.0f, -4.0f});
    REQUIRE(negative_w.x == -0.75f);
    REQUIRE(negative_w.y == 1.5f);
    REQUIRE(negative_w.z == -3.0f);
}


TEST_CASE("Vec4 names a point only up to scale", "[Vec4]")
{
    // The defining property of homogeneous coordinates: scaling all four
    // components leaves the point unchanged, so operator* on a point is a
    // no-op once the divide runs. It only scales genuine directions (w == 0).
    tinymath::Vec4f const point{0.5f, -1.5f, 2.0f, 1.0f};

    tinymath::Vec3f original = toVec3(point);
    tinymath::Vec3f scaled = toVec3(point * 4.0f);

    REQUIRE(scaled.x == original.x);
    REQUIRE(scaled.y == original.y);
    REQUIRE(scaled.z == original.z);
}


TEST_CASE("toVec4 and toVec3 round-trip", "[Vec4]")
{
    tinymath::Vec3f const original{0.5f, -1.75f, 2.25f};

    tinymath::Vec3f round_tripped = toVec3(toVec4(original));
    REQUIRE(round_tripped.x == original.x);
    REQUIRE(round_tripped.y == original.y);
    REQUIRE(round_tripped.z == original.z);
}
