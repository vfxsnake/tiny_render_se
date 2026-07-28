#pragma once

#include <cstdint>

struct Color
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;

    // default == operator, generates a member-wise comparison.
    bool operator==(const Color& other) const = default;
};

static_assert(sizeof(Color) == 4, "Color must be tightly packed RGBA for GPU upload" );
