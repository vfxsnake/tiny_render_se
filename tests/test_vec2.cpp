#include <catch2/catch_test_macros.hpp>

#include "math/Vec2.h"

TEST_CASE("Vec2 initialization and templating check", "[Vec2]")
{
    Vec2<int> vec2_int{10, 20};
    REQUIRE(vec2_int.x == 10);
    REQUIRE(vec2_int.y == 20);

    Vec2<float> vec2_float{0.5f, -1.75f};
    REQUIRE(vec2_float.x == 0.5f);
    REQUIRE(vec2_float.y == -1.75f);
}
