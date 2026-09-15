#pragma once

#include <string>

#include "rasterizer/Texture.h"

namespace io
{

    Texture loadTexture(std::string const& path);

} // namespace io