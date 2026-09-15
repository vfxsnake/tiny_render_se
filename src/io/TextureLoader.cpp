#include "TextureLoader.h"

#include <stdexcept>
#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"



Texture io::loadTexture(std::string const& path)
{
    int width;
    int height;
    
    unsigned char* image = stbi_load(
        path.c_str(), 
        &width,  
        &height, 
        nullptr, // channels reference wont be used, as we are using Texture::CHANNELS. 
        Texture::CHANNELS
    );

    if (!image)
    {
        throw std::runtime_error("unable to load texture: " + path);
    }

    std::vector<uint8_t> pixels(image, image  + (width * height * Texture::CHANNELS));
    
    stbi_image_free(image);
    
    return Texture(std::move(pixels), width, height);
}