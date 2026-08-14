#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "math/Projection.h"
#include "math/Vec4.h"

namespace
{
    // Deliberately non-square: a square viewport cannot tell a swapped x/y
    // scale from a correct one.
    constexpr int SCREEN_WIDTH = 800;
    constexpr int SCREEN_HEIGHT = 600;

    constexpr float TOLERANCE = 1e-5f;
}


TEST_CASE("Viewport maps the NDC cube corners onto the screen", "[Projection]")
{
    tinymath::Matrix4x4 const to_screen = tinymath::viewport(SCREEN_WIDTH, SCREEN_HEIGHT);

    // NDC x and y run -1..1 and must land on 0..width and 0..height.
    tinymath::Vec4f const low_corner = to_screen * tinymath::Vec4f{-1.0f, -1.0f, -1.0f, 1.0f};
    REQUIRE_THAT(low_corner.x, Catch::Matchers::WithinAbs(0.0f, TOLERANCE));
    REQUIRE_THAT(low_corner.y, Catch::Matchers::WithinAbs(0.0f, TOLERANCE));

    tinymath::Vec4f const high_corner = to_screen * tinymath::Vec4f{1.0f, 1.0f, 1.0f, 1.0f};
    REQUIRE_THAT(high_corner.x, Catch::Matchers::WithinAbs(800.0f, TOLERANCE));
    REQUIRE_THAT(high_corner.y, Catch::Matchers::WithinAbs(600.0f, TOLERANCE));

    // The centre of the cube is the centre of the screen. Different values for
    // x and y, so an index swap cannot pass.
    tinymath::Vec4f const centre = to_screen * tinymath::Vec4f{0.0f, 0.0f, 0.0f, 1.0f};
    REQUIRE_THAT(centre.x, Catch::Matchers::WithinAbs(400.0f, TOLERANCE));
    REQUIRE_THAT(centre.y, Catch::Matchers::WithinAbs(300.0f, TOLERANCE));
}


TEST_CASE("Viewport rescales depth into 0..1 instead of passing it through", "[Projection]")
{
    // This row is a deliberate departure from the lesson, which leaves z alone.
    // Framebuffer clears depth to 0.0f and keeps the larger value, so a z still
    // running -1..1 would put the entire back half of a model below the clear
    // and lose it in silence.
    tinymath::Matrix4x4 const to_screen = tinymath::viewport(SCREEN_WIDTH, SCREEN_HEIGHT);

    tinymath::Vec4f const near_plane = to_screen * tinymath::Vec4f{0.0f, 0.0f, -1.0f, 1.0f};
    REQUIRE_THAT(near_plane.z, Catch::Matchers::WithinAbs(0.0f, TOLERANCE));

    tinymath::Vec4f const far_plane = to_screen * tinymath::Vec4f{0.0f, 0.0f, 1.0f, 1.0f};
    REQUIRE_THAT(far_plane.z, Catch::Matchers::WithinAbs(1.0f, TOLERANCE));

    tinymath::Vec4f const middle = to_screen * tinymath::Vec4f{0.0f, 0.0f, 0.0f, 1.0f};
    REQUIRE_THAT(middle.z, Catch::Matchers::WithinAbs(0.5f, TOLERANCE));
}


TEST_CASE("Viewport leaves the homogeneous coordinate alone", "[Projection]")
{
    // Viewport is a scale and an offset. If it touched w it would trigger a
    // second, unwanted divide downstream.
    tinymath::Matrix4x4 const to_screen = tinymath::viewport(SCREEN_WIDTH, SCREEN_HEIGHT);

    tinymath::Vec4f const result = to_screen * tinymath::Vec4f{0.25f, -0.5f, 0.75f, 1.0f};
    REQUIRE_THAT(result.w, Catch::Matchers::WithinAbs(1.0f, TOLERANCE));
}


TEST_CASE("Perspective writes w and touches nothing else", "[Projection]")
{
    // The whole matrix is one entry. It scales nothing — the shrink happens
    // only when something divides by w. Feeding its output straight into a
    // Triangle without calling toVec3 does not look broken, it looks like a
    // parallel projection, which is exactly how that bug survived review once.
    tinymath::Matrix4x4 const projection = tinymath::perspective(3.0f);

    tinymath::Vec4f const result = projection * tinymath::Vec4f{2.0f, 3.0f, 1.0f, 1.0f};
    REQUIRE_THAT(result.x, Catch::Matchers::WithinAbs(2.0f, TOLERANCE));
    REQUIRE_THAT(result.y, Catch::Matchers::WithinAbs(3.0f, TOLERANCE));
    REQUIRE_THAT(result.z, Catch::Matchers::WithinAbs(1.0f, TOLERANCE));
}


TEST_CASE("Perspective sets w to 1 - z / focal_length", "[Projection]")
{
    tinymath::Matrix4x4 const projection = tinymath::perspective(3.0f);

    // On the pivot plane nothing happens: w stays 1, so the divide is a no-op
    // and geometry at z == 0 is neither grown nor shrunk.
    tinymath::Vec4f const on_pivot = projection * tinymath::Vec4f{1.0f, 1.0f, 0.0f, 1.0f};
    REQUIRE_THAT(on_pivot.w, Catch::Matchers::WithinAbs(1.0f, TOLERANCE));

    // Nearer than the pivot plane: w < 1, so the divide magnifies.
    tinymath::Vec4f const nearer = projection * tinymath::Vec4f{1.0f, 1.0f, 1.0f, 1.0f};
    REQUIRE_THAT(nearer.w, Catch::Matchers::WithinAbs(2.0f / 3.0f, TOLERANCE));

    // Further away: w > 1, so the divide shrinks. This is why the perspective
    // divide compresses the far side and protects the depth invariant.
    tinymath::Vec4f const further = projection * tinymath::Vec4f{1.0f, 1.0f, -3.0f, 1.0f};
    REQUIRE_THAT(further.w, Catch::Matchers::WithinAbs(2.0f, TOLERANCE));
}


TEST_CASE("Perspective drives w to zero at the eye", "[Projection]")
{
    // z == focal_length is the eye position itself, where the projection is
    // genuinely undefined. Recorded rather than guarded: toVec3 divides by w
    // unchecked, and this is the only input that reaches it.
    tinymath::Matrix4x4 const projection = tinymath::perspective(3.0f);

    tinymath::Vec4f const at_eye = projection * tinymath::Vec4f{1.0f, 1.0f, 3.0f, 1.0f};
    REQUIRE_THAT(at_eye.w, Catch::Matchers::WithinAbs(0.0f, TOLERANCE));
}


TEST_CASE("Perspective divide magnifies what is nearer than the pivot plane", "[Projection]")
{
    // The visible consequence, end to end: same x and y, different z, and the
    // nearer point comes out further from the centre of the screen.
    tinymath::Matrix4x4 const projection = tinymath::perspective(3.0f);

    tinymath::Vec3f const nearer = tinymath::toVec3(
        projection * tinymath::Vec4f{1.0f, 1.0f, 1.0f, 1.0f}
    );
    REQUIRE_THAT(nearer.x, Catch::Matchers::WithinAbs(1.5f, TOLERANCE));
    REQUIRE_THAT(nearer.y, Catch::Matchers::WithinAbs(1.5f, TOLERANCE));

    tinymath::Vec3f const further = tinymath::toVec3(
        projection * tinymath::Vec4f{1.0f, 1.0f, -3.0f, 1.0f}
    );
    REQUIRE_THAT(further.x, Catch::Matchers::WithinAbs(0.5f, TOLERANCE));
    REQUIRE_THAT(further.y, Catch::Matchers::WithinAbs(0.5f, TOLERANCE));
}
