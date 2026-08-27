#pragma once

#include <string>

#include "math/Vec3.h"
#include "geometry/Mesh.h"

namespace io
{

    Mesh loadObj(std::string const& path);

} // namespace io

