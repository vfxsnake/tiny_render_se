#include "Texture.h"

#include <utility>
#include <algorithm>
#include <cassert>

Texture::Texture(
    std::vector<uint8_t> pixels, int width, int height
):
    pixels_(std::move(pixels)),
    width_(width),
    height_(height)
{
    assert(pixels_.size() == static_cast<size_t>(width_ * height_ * CHANNELS));
}

Color Texture::sample(float u, float v) const
{
    int texture_x = static_cast<int>(u * width_);
    int texture_y = static_cast<int>(v * height_);
    texture_x = std::clamp(texture_x, 0, width_ - 1);
    texture_y = std::clamp(texture_y, 0, height_ - 1);
    texture_y = (height_ - 1) - texture_y;  // y flip, as texture first index is top lef corner
    int index = ((texture_y * width_) + texture_x) * CHANNELS;

    return {
        pixels_[index],     // r
        pixels_[index + 1], // g
        pixels_[index + 2], // b
        pixels_[index + 3]  // a
    };
}