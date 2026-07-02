#include <catch2/catch_test_macros.hpp>
#include "rasterizer/Framebuffer.h"

TEST_CASE("Framebuffer check dimensions", "[framebuffer]")
{
    Framebuffer fb(4,4);
    REQUIRE(fb.getWidth() == 4);
    REQUIRE(fb.getHeight() == 4);
}


TEST_CASE("Framebuffer clear color check", "[framebuffer]")
{
    Framebuffer fb(2,2);
    fb.clear(Color{255, 128, 0, 1});
    const uint8_t* pixels = fb.getData();
    const int bytes_per_pixel = sizeof(Color);
    const int total_bytes = fb.getWidth() * fb.getHeight() * bytes_per_pixel;
    for (int i=0; i < total_bytes; i+=bytes_per_pixel)
    {
        REQUIRE(pixels[i] == 255);
        REQUIRE(pixels[i + 1] == 128);
        REQUIRE(pixels[i + 2] == 0);
        REQUIRE(pixels[i + 3] == 1);
    }
}


TEST_CASE("Framebuffer set color at valid range (index) check", "[framebuffer]")
{
    Framebuffer fb(3,2);
    fb.setPixel(0,0, Color{255,128,0,1}); // pixel index 0
    fb.setPixel(1,1, Color{255,128,0,1}); // pixel index 4
    fb.setPixel(2,0, Color{255,128,0,1}); // pixel index 2

    const uint8_t* pixels = fb.getData();
    const int bytes_per_pixel = sizeof(Color);
    const int total_bytes = fb.getWidth() * fb.getHeight() * bytes_per_pixel;
    for (int i=0; i < total_bytes; i += bytes_per_pixel)
    {
        int index = i / bytes_per_pixel;
        if (index == 0 || index == 2 || index == 4)
        {
            REQUIRE(pixels[i] == 255);
            REQUIRE(pixels[i + 1] == 128);
            REQUIRE(pixels[i + 2] == 0);
            REQUIRE(pixels[i + 3] == 1);
        }
        else
        {
            REQUIRE(pixels[i] == 0);
            REQUIRE(pixels[i + 1] == 0);
            REQUIRE(pixels[i + 2] == 0);
            REQUIRE(pixels[i + 3] == 0);
        }
    }
}


TEST_CASE("Framebuffer out of bounds prevents setting color check", "[framebuffer]")
{
    Framebuffer fb(2,2);
    // out of bounds on y
    fb.setPixel(0,-1, Color{255,128,1,10});
    fb.setPixel(0, 2, Color{255,128,1,10});
    
    // out of bounds on z
    fb.setPixel(-1,0, Color{255,128,1,10});
    fb.setPixel(2,0, Color{255,128,1,10});

    // out of bound at both axes
    fb.setPixel(-1,-1, Color{255,128,1,10});
    fb.setPixel(2,2, Color{255,128,1,10});

    
    const uint8_t* pixels = fb.getData();
    const int bytes_per_pixel = sizeof(Color);
    const int total_bytes = fb.getWidth() * fb.getHeight() * bytes_per_pixel;
    
    for (int i=0; i < total_bytes; i+=bytes_per_pixel)
    {
        REQUIRE(pixels[i] == 0);
        REQUIRE(pixels[i + 1] == 0);
        REQUIRE(pixels[i + 2] == 0);
        REQUIRE(pixels[i + 3] == 0);
    }
}


TEST_CASE("Framebuffer set depth at valid range (index) check", "[framebuffer]")
{
    Framebuffer fb(3,2);
    fb.setDepth(0,0, 10.0f); // pixel index 0
    fb.setDepth(1,1, 10.0f); // pixel index 4
    fb.setDepth(2,0, 10.0f); // pixel index 2

    int width = fb.getWidth();
    int height = fb.getHeight();

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float depth_value = fb.getDepth(x, y);
            int index = y * width + x;
            if (index == 0 || index == 2 || index == 4)
            {
                REQUIRE(depth_value == 10.0f);
            }
            else
            {
                REQUIRE(depth_value == 0.0f);
            }
        }
    }
}


TEST_CASE("Framebuffer get_depth check with bounds", "[framebuffer]")
{
    Framebuffer fb(2,2);

    fb.setDepth(0,0, 10.0f);  // index 0
    fb.setDepth(1, 0, 20.0f); // index 1
    fb.setDepth(0, 1, 30.0f); // index 2
    fb.setDepth(1, 1, 40.0f); // index 3

    REQUIRE(fb.getDepth(0, 0) == 10.0f);
    REQUIRE(fb.getDepth(1, 0) == 20.0f);
    REQUIRE(fb.getDepth(0, 1) == 30.0f);
    REQUIRE(fb.getDepth(1, 1) == 40.0f);
    
    // out of bounds in x
    REQUIRE(fb.getDepth(-1, 0) == 0.0f);
    REQUIRE(fb.getDepth(2, 0) == 0.0f);
    
    // out of bounds in y
    REQUIRE(fb.getDepth(0, -1) == 0.0f);
    REQUIRE(fb.getDepth(0, 2) == 0.0f);

    // out of bound in both
    REQUIRE(fb.getDepth(-1, 2) == 0.0f);
    REQUIRE(fb.getDepth(2, -1) == 0.0f);
}