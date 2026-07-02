#pragma once

#include <cstdint>
#include <vector>

#include "Color.h"


class Framebuffer
{
public:
    Framebuffer(int width, int height);

    void clear(const Color& color);
    
    void setPixel(int x, int y, const Color& color);
    void setDepth(int x, int y, float depth);
    
    float getDepth(int x, int y) const;
    int getWidth() const;
    int getHeight() const;

    const uint8_t* getData() const;

private:
    int index(int x, int y) const;
    bool inBounds(int x, int y) const;

    int width_;
    int height_;

    std::vector<Color> pixels_;
    std::vector<float> depth_;
};