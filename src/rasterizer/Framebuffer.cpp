#include "Framebuffer.h"

#include <algorithm>

Framebuffer::Framebuffer(int width, int height) : 
    width_(width), 
    height_(height),
    pixels_(static_cast<std::size_t>(width) * height),
    depth_(static_cast<std::size_t>(width) * height, 0.0f)
{

}


int Framebuffer::index(int x, int y) const
{
    return ((y * width_) + x);
}


bool Framebuffer::inBounds(int x, int y) const
{
    return ((x >= 0 && x < width_) && (y >= 0 && y < height_));
}


void Framebuffer::setPixel(int x, int y, const Color& color)
{
    if (!inBounds(x, y)) return;  // this check can be moved out later for optimizing redundant checks
    
    pixels_[index(x, y)] = color;
}

void Framebuffer::setDepth(int x, int y, float depth)
{
    if (!inBounds(x, y)) return; // this check can be moved out later for optimizing redundant checks

    depth_[index(x,y)] = depth;
}


float Framebuffer::getDepth(int x, int y) const
{
    if(!inBounds(x, y)) return 0.0f; 
    return depth_[index(x, y)];
}

int Framebuffer::getWidth() const
{
    return width_;
}


int Framebuffer::getHeight() const
{
    return height_;
}

const uint8_t* Framebuffer::getData() const
{
    return reinterpret_cast<const uint8_t*>(pixels_.data());
}


void Framebuffer::clear(const Color& color)
{
    std::fill(pixels_.begin(), pixels_.end(), color);
    std::fill(depth_.begin(), depth_.end(), 0.0f);
}