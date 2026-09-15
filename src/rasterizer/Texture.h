#pragma once 

#include <cstdint>
#include <vector>

#include "Color.h"


class Texture
{
public:
    Texture(std::vector<uint8_t> pixels, int width, int height);
    Color sample(float u, float v) const;
    
    static constexpr int CHANNELS = 4;

private:
    std::vector<uint8_t> pixels_;
    int width_ = 0;
    int height_ = 0;
};
