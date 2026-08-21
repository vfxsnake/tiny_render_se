#pragma once

#include <vector>
#include <array>
#include <string>

#include "math/Vec3.h"

namespace io
{
    struct Mesh
    {
        std::vector<tinymath::Vec3f> vertices;
        std::vector<tinymath::Vec3f> normals;

        std::vector<std::array<int,3>> faceIndices;
        std::vector<std::array<int,3>> faceNormalIndices;
    };

    Mesh loadObj(std::string const& path);

} // namespace io

