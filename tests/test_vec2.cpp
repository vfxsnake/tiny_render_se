#include <catch2/catch_test_macros.hpp>

#include "math/Vec2.h"

TEST_CASE("Vec2 initialization and templating check", "[Vec2]")
{
    tinymath::Vec2<int> vec2_int{10, 20};
    REQUIRE(vec2_int.x == 10);
    REQUIRE(vec2_int.y == 20);

    tinymath::Vec2<float> vec2_float{0.5f, -1.75f};
    REQUIRE(vec2_float.x == 0.5f);
    REQUIRE(vec2_float.y == -1.75f);
}


TEST_CASE("Vec2 subtraction", "[Vec2]")
{
    tinymath::Vec2i a{10, 20};
    tinymath::Vec2i b{3, 8};

    tinymath::Vec2i diff = a - b;
    REQUIRE(diff.x == 7);
    REQUIRE(diff.y == 12);
}


TEST_CASE("Vec2 cross is the signed area of the parallelogram", "[Vec2]")
{
    // +x cross +y = +1 (right-handed, positive area).
    REQUIRE(cross(tinymath::Vec2i{1, 0}, tinymath::Vec2i{0, 1}) == 1);

    // Swapping the arguments flips the sign.
    REQUIRE(cross(tinymath::Vec2i{0, 1}, tinymath::Vec2i{1, 0}) == -1);

    // Collinear (parallel) vectors enclose no area.
    REQUIRE(cross(tinymath::Vec2i{2, 4}, tinymath::Vec2i{1, 2}) == 0);

    // Known value: 3*4 - 2*5 = 2.
    REQUIRE(cross(tinymath::Vec2i{3, 2}, tinymath::Vec2i{5, 4}) == 2);
}
