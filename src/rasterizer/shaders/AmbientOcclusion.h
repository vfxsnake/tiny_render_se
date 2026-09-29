#pragma once

#include <vector>

#include "math/Vec3.h"
#include "math/Matrix4x4.h"

// forward declarations
class Framebuffer;

namespace AmbientOcclusion
{
    float estimateAmbientOcclusion(
        tinymath::Vec3f position,
        tinymath::Vec3f normal,
        const std::vector<tinymath::Vec3f>& occlusion_sample_vectors,
        float radius,
        float bias,
        const tinymath::Matrix4x4& lookup_matrix,
        const Framebuffer& depth_frame_buffer
    );
} // end of AmbientOcclusion names space

