#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "rasterizer/Color.h"
#include "rasterizer/Texture.h"


namespace
{
    /*
        A 2x2 fixture whose four texels are all distinct in every channel, so a single
        sample identifies both the texel that was read and the order its bytes came back in.

        stb hands back the image top-down, so the first row in memory is the TOP of the image
        while v = 0 is its BOTTOM. The names below are image positions, not memory order.
    */
    const Color TOP_LEFT{10, 11, 12, 13};
    const Color TOP_RIGHT{20, 21, 22, 23};
    const Color BOTTOM_LEFT{30, 31, 32, 33};
    const Color BOTTOM_RIGHT{40, 41, 42, 43};

    std::vector<uint8_t> checkerBytes()
    {
        return {
            10, 11, 12, 13,   20, 21, 22, 23,  // first row in memory = top of the image
            30, 31, 32, 33,   40, 41, 42, 43   // second row in memory = bottom of the image
        };
    }

    Texture checkerTexture()
    {
        return Texture(checkerBytes(), 2, 2);
    }
}


TEST_CASE("v = 0 samples the bottom of the image, not the first row in memory", "[texture][flip]")
{
    const Texture texture = checkerTexture();

    /*
        The whole v-flip lives here. Without it these two assertions return the top row,
        which is what put diablo's diffuse map on screen upside down.
    */
    REQUIRE(texture.sample(0.0f, 0.0f) == BOTTOM_LEFT);
    REQUIRE(texture.sample(0.9f, 0.0f) == BOTTOM_RIGHT);

    REQUIRE(texture.sample(0.0f, 0.9f) == TOP_LEFT);
    REQUIRE(texture.sample(0.9f, 0.9f) == TOP_RIGHT);
}


TEST_CASE("the four bytes come back as r, g, b, a in that order", "[texture][channels]")
{
    const Texture texture = checkerTexture();

    /*
        Every channel of every texel is a different number, so a swapped or misindexed
        channel cannot coincide with the right answer. Indexing pixels_[index] + 3 rather
        than pixels_[index + 3] returns blue plus three, which is a plausible-looking alpha.
    */
    const Color sampled = texture.sample(0.0f, 0.0f);

    REQUIRE(sampled.r == 30);
    REQUIRE(sampled.g == 31);
    REQUIRE(sampled.b == 32);
    REQUIRE(sampled.a == 33);
}


TEST_CASE("u and v of exactly 1.0 stay inside the image", "[texture][clamp]")
{
    const Texture texture = checkerTexture();

    /*
        Diablo's uvs reach 1.0 inclusive, and 1.0f * width_ is exactly width_ -- one texel
        past the end of the row. The clamp sits on the integer for this case alone: clamping
        the incoming float to 1.0 does not move it.

        Without the clamp, sample(1.0f, 1.0f) indexes byte 16 of a 16-byte vector.
    */
    REQUIRE(texture.sample(1.0f, 1.0f) == TOP_RIGHT);
    REQUIRE(texture.sample(1.0f, 0.0f) == BOTTOM_RIGHT);
    REQUIRE(texture.sample(0.0f, 1.0f) == TOP_LEFT);
}


TEST_CASE("a texel owns the half-open range that truncates into it", "[texture][boundary]")
{
    const Texture texture = checkerTexture();

    /*
        Nearest sampling by truncation: on a 2-wide texture texel 0 owns [0.0, 0.5) and
        texel 1 owns [0.5, 1.0]. This is the rule bilinear filtering would replace.
    */
    REQUIRE(texture.sample(0.499f, 0.0f) == BOTTOM_LEFT);
    REQUIRE(texture.sample(0.5f, 0.0f) == BOTTOM_RIGHT);
}


TEST_CASE("a non-square texture indexes rows by width, not by height", "[texture][index]")
{
    /*
        A square fixture cannot catch a transposed index, because (y * width + x) and
        (x * height + y) agree whenever width == height. This one is 2 wide and 3 tall.
    */
    const std::vector<uint8_t> pixels = {
        1, 1, 1, 1,   2, 2, 2, 2,  // top row in memory
        3, 3, 3, 3,   4, 4, 4, 4,  // middle row
        5, 5, 5, 5,   6, 6, 6, 6   // bottom row in memory
    };

    const Texture texture(pixels, 2, 3);

    // top-right: correct index 1, a transposed index would land on byte 3
    REQUIRE(texture.sample(0.9f, 1.0f) == Color{2, 2, 2, 2});

    REQUIRE(texture.sample(0.0f, 1.0f) == Color{1, 1, 1, 1});
    REQUIRE(texture.sample(0.0f, 0.5f) == Color{3, 3, 3, 3});
    REQUIRE(texture.sample(0.0f, 0.0f) == Color{5, 5, 5, 5});
}


TEST_CASE("a 1x1 texture returns its only texel for every uv", "[texture][clamp]")
{
    const Texture texture(std::vector<uint8_t>{7, 8, 9, 10}, 1, 1);

    REQUIRE(texture.sample(0.0f, 0.0f) == Color{7, 8, 9, 10});
    REQUIRE(texture.sample(1.0f, 1.0f) == Color{7, 8, 9, 10});
    REQUIRE(texture.sample(0.5f, 0.5f) == Color{7, 8, 9, 10});
}
