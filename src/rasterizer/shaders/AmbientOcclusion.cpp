#include "AmbientOcclusion.h"

#include <cmath>

#include "rasterizer/Framebuffer.h"

float AmbientOcclusion::estimateAmbientOcclusion(
    tinymath::Vec3f position,
    tinymath::Vec3f normal,
    const std::vector<tinymath::Vec3f>& occlusion_sample_vectors,
    float radius,
    float bias,
    const tinymath::Matrix4x4& lookup_matrix,
    const Framebuffer& depth_frame_buffer
)
{
    int open_samples = 0;

    for (tinymath::Vec3f sample_vector : occlusion_sample_vectors)
    {
        
        if (tinymath::dot(normal, sample_vector) < 0.0f)
        {
            sample_vector = sample_vector * -1.0f;
        }

        tinymath::Vec3f sample_point = position + (sample_vector * radius);

        tinymath::Vec3f projected_point = tinymath::toVec3(
            lookup_matrix * tinymath::toVec4(sample_point)
        );

        float stored_depth = depth_frame_buffer.getDepth(
            static_cast<int>(std::round(projected_point.x)),
            static_cast<int>(std::round(projected_point.y))
        );

        if (stored_depth <= (projected_point.z + bias))
        {
            open_samples++;
        }
    }

    return open_samples / static_cast<float>(occlusion_sample_vectors.size());
}