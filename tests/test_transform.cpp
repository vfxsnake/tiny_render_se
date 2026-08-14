#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "math/Transform.h"
#include "math/Vec4.h"

namespace
{
    constexpr float PI = 3.14159265f;
    constexpr float TOLERANCE = 1e-5f;
}


TEST_CASE("rotateY swings +z toward +x", "[Transform]")
{
    // The direction matters and is not guessable from the signature. It is what
    // fixes which side the camera has to stand on to reproduce the spike: this
    // rotation walks the model's front toward +x, so the equivalent eye is at -x.
    tinymath::Vec3f const rotated = tinymath::rotateY({0.0f, 0.0f, 1.0f}, PI * 0.5f);

    REQUIRE_THAT(rotated.x, Catch::Matchers::WithinAbs(1.0f, TOLERANCE));
    REQUIRE_THAT(rotated.z, Catch::Matchers::WithinAbs(0.0f, TOLERANCE));
}


TEST_CASE("rotateY leaves the axis of rotation untouched", "[Transform]")
{
    tinymath::Vec3f const rotated = tinymath::rotateY({1.0f, 5.0f, 2.0f}, 0.7f);

    REQUIRE_THAT(rotated.y, Catch::Matchers::WithinAbs(5.0f, TOLERANCE));

    // A rotation is rigid: the distance from the y axis cannot change.
    float const radius_before = std::sqrt(1.0f * 1.0f + 2.0f * 2.0f);
    float const radius_after = std::sqrt(rotated.x * rotated.x + rotated.z * rotated.z);
    REQUIRE_THAT(radius_after, Catch::Matchers::WithinAbs(radius_before, TOLERANCE));
}


TEST_CASE("lookAt from an on-axis eye is the identity", "[Transform]")
{
    // Eye on +z, target at the origin: the camera basis already agrees with the
    // world axes and there is nothing to translate. Worth pinning because it is
    // the smoke-test camera — and because it proves nothing about the basis
    // convention, since identity is its own transpose.
    tinymath::Matrix4x4 const view = tinymath::lookAt(
        {0.0f, 0.0f, 3.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    );

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            float const expected = (i == j) ? 1.0f : 0.0f;
            REQUIRE_THAT(view.data[i][j], Catch::Matchers::WithinAbs(expected, TOLERANCE));
        }
    }
}


TEST_CASE("lookAt puts the reference point at the camera-space origin", "[Transform]")
{
    // Note this translates by -target, not -eye, so it is not a standard view
    // matrix: the model lands centred on the origin and the eye's distance is
    // carried entirely by perspective's focal length.
    tinymath::Vec3f const target{1.0f, 0.0f, 0.0f};

    tinymath::Matrix4x4 const view = tinymath::lookAt(
        {2.0f, 0.0f, 0.0f},
        target,
        {0.0f, 1.0f, 0.0f}
    );

    tinymath::Vec4f const at_origin = view * tinymath::toVec4(target);
    REQUIRE_THAT(at_origin.x, Catch::Matchers::WithinAbs(0.0f, TOLERANCE));
    REQUIRE_THAT(at_origin.y, Catch::Matchers::WithinAbs(0.0f, TOLERANCE));
    REQUIRE_THAT(at_origin.z, Catch::Matchers::WithinAbs(0.0f, TOLERANCE));
    REQUIRE_THAT(at_origin.w, Catch::Matchers::WithinAbs(1.0f, TOLERANCE));
}


TEST_CASE("lookAt writes the camera basis into rows", "[Transform]")
{
    // Camera on +x looking back at the origin. Its right vector is -z, so a
    // point sitting at +z is to the camera's LEFT and must come out at negative x.
    //
    // This is the case that discriminates the convention. Writing the same
    // basis into the columns builds the transpose, which for an orthonormal
    // basis is the inverse, and this point would come out at x = +1 instead —
    // a rendered image that looks entirely plausible. Confirmed on screen in
    // lesson 5 before this test was written; the test only locks it in.
    tinymath::Matrix4x4 const view = tinymath::lookAt(
        {2.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    );

    tinymath::Vec4f const result = view * tinymath::toVec4({1.0f, 0.0f, 1.0f});
    REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(-1.0f, TOLERANCE));
    REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(0.0f, TOLERANCE));
}


TEST_CASE("lookAt applies the offset before the rotation", "[Transform]")
{
    // Same camera and same point as above. Composed the wrong way round
    // (offset * rotation) the point lands at (-2, 0, 1) instead of (-1, 0, 0):
    // the rotation would spin it about the world origin rather than about the
    // reference point, so it comes out off to the side and at the wrong depth.
    tinymath::Matrix4x4 const view = tinymath::lookAt(
        {2.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    );

    tinymath::Vec4f const result = view * tinymath::toVec4({1.0f, 0.0f, 1.0f});
    REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(-1.0f, TOLERANCE));
    REQUIRE_THAT(result.z, Catch::Matchers::WithinAbs(0.0f, TOLERANCE));
}


TEST_CASE("lookAt treats up as a hint, not a constraint", "[Transform]")
{
    // up need not be perpendicular to the view direction: cross(up, n) then
    // cross(n, l) re-orthogonalises it. A tilted up that spans the same plane
    // must therefore produce exactly the same matrix. What up must never be is
    // parallel to the view direction — that is the degenerate case normalize
    // now asserts on.
    tinymath::Matrix4x4 const clean_up = tinymath::lookAt(
        {0.0f, 0.0f, 3.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    );

    tinymath::Matrix4x4 const tilted_up = tinymath::lookAt(
        {0.0f, 0.0f, 3.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 1.0f}
    );

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            REQUIRE_THAT(
                tilted_up.data[i][j],
                Catch::Matchers::WithinAbs(clean_up.data[i][j], TOLERANCE)
            );
        }
    }
}


TEST_CASE("lookAt produces an orthonormal basis", "[Transform]")
{
    // An off-axis camera, so all three rows are dense and a misplaced component
    // cannot hide in a zero.
    tinymath::Matrix4x4 const view = tinymath::lookAt(
        {1.0f, 2.0f, 3.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    );

    for (int row = 0; row < 3; row++)
    {
        float const length_squared =
            view.data[row][0] * view.data[row][0] +
            view.data[row][1] * view.data[row][1] +
            view.data[row][2] * view.data[row][2];

        REQUIRE_THAT(length_squared, Catch::Matchers::WithinAbs(1.0f, TOLERANCE));
    }

    // Mutually perpendicular. This is the property that makes the transpose the
    // inverse, which is why getting the convention wrong stays invisible.
    for (int first = 0; first < 3; first++)
    {
        for (int second = first + 1; second < 3; second++)
        {
            float const dot_product =
                view.data[first][0] * view.data[second][0] +
                view.data[first][1] * view.data[second][1] +
                view.data[first][2] * view.data[second][2];

            REQUIRE_THAT(dot_product, Catch::Matchers::WithinAbs(0.0f, TOLERANCE));
        }
    }
}
