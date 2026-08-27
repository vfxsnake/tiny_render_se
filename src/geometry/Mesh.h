#pragma once

#include <array>
#include <vector>

#include "math/Vec3.h"

struct Mesh
{
    std::vector<tinymath::Vec3f> vertices;
    std::vector<tinymath::Vec3f> normals;

    std::vector<std::array<int,3>> faceIndices;
    std::vector<std::array<int,3>> faceNormalIndices;
};
