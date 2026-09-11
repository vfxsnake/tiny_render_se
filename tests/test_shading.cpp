#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <array>
#include <cmath>

#include "geometry/Mesh.h"
#include "math/Matrix4x4.h"
#include "math/Vec3.h"
#include "math/Vec4.h"
#include "rasterizer/Color.h"
#include "rasterizer/Framebuffer.h"
#include "rasterizer/ScreenSpace.h"
#include "rasterizer/TriangleRasterizer.h"
#include "rasterizer/shaders/AbstractShader.h"
#include "rasterizer/shaders/BlinnPhongShader.h"
#include "rasterizer/shaders/FaceShader.h"
#include "rasterizer/shaders/GouraudShader.h"
#include "rasterizer/shaders/LambertShader.h"
#include "rasterizer/shaders/PhongShader.h"


namespace
{
    const Color BASE{200, 200, 200, 255};
    const Color BLACK_BASE{0, 0, 0, 255};
    const Color BLACK{0, 0, 0, 0};
    const Color RED{255, 0, 0, 255};
    const Color BLUE{0, 0, 255, 255};

    const float PI = 3.14159265f;

    // A channel is a float truncated to 8 bits, so wherever the exact value is not a
    // whole number the shader output is compared with one unit of slack.
    const float CHANNEL_MARGIN = 1.0f;

    const screen::BarycentricWeights AT_A{1.0f, 0.0f, 0.0f};
    const screen::BarycentricWeights AT_B{0.0f, 1.0f, 0.0f};
    const screen::BarycentricWeights AT_C{0.0f, 0.0f, 1.0f};
    const screen::BarycentricWeights CENTROID{1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f};

    // One triangle in the z = 0 plane, wound counter-clockwise seen from +z, so its
    // cross-product normal is {0, 0, 1}. Each corner carries its own vn, as an OBJ
    // face does.
    Mesh makeTriangleMesh(tinymath::Vec3f normal_a, tinymath::Vec3f normal_b, tinymath::Vec3f normal_c)
    {
        Mesh mesh;
        mesh.vertices = {
            tinymath::Vec3f{-0.75f, -0.75f, 0.0f},
            tinymath::Vec3f{0.75f, -0.75f, 0.0f},
            tinymath::Vec3f{-0.75f, 0.75f, 0.0f}
        };
        mesh.normals = {normal_a, normal_b, normal_c};
        mesh.faceIndices.push_back({0, 1, 2});
        mesh.faceNormalIndices.push_back({0, 1, 2});
        return mesh;
    }

    // Unit vector in the xz plane, the angle measured from +z toward +x.
    tinymath::Vec3f fromDegrees(float degrees)
    {
        float radians = degrees * PI / 180.0f;
        return {std::sin(radians), 0.0f, std::cos(radians)};
    }

    // Runs the vertex stage for the mesh's only face, in index order so shaders with
    // an ordering contract are satisfied, then the fragment stage at the given weights.
    Color shade(AbstractShader& shader, screen::BarycentricWeights weights)
    {
        for (int vertex_index = 0; vertex_index < 3; vertex_index++)
        {
            shader.vertex(0, vertex_index);
        }

        Color out_color;
        shader.fragment(weights, out_color);
        return out_color;
    }

    /*
        Test-only shader: the screen-space right triangle (2,2), (14,2), (2,14) on a
        16x16 framebuffer, at one constant depth. The corners are returned directly in
        clip space with w = 1, so the rasterizer's viewport maps x and y by * 8 + 8 and z
        by * 0.5 + 0.5.
    */
    class ConstantShader : public AbstractShader
    {
    public:
        ConstantShader(float depth, Color color, bool keep_fragment) :
            ndcZ_(depth * 2.0f - 1.0f),
            color_(color),
            keepFragment_(keep_fragment)
        {

        }

        tinymath::Vec4f vertex(int, int vertex_index) override
        {
            const std::array<tinymath::Vec4f, 3> corners = {{
                {-0.75f, -0.75f, ndcZ_, 1.0f},
                {0.75f, -0.75f, ndcZ_, 1.0f},
                {-0.75f, 0.75f, ndcZ_, 1.0f}
            }};
            return corners[vertex_index];
        }

        bool fragment(screen::BarycentricWeights, Color& out_color) override
        {
            // Written even when discarding, so a rasterizer that ignored the return
            // value would show this colour.
            out_color = color_;
            return keepFragment_;
        }

    private:
        float ndcZ_;
        Color color_;
        bool keepFragment_;
    };

    void drawWithShader(AbstractShader& shader, Framebuffer& frame_buffer)
    {
        std::array<tinymath::Vec4f, 3> clip_positions;
        clip_positions[0] = shader.vertex(0, 0);
        clip_positions[1] = shader.vertex(0, 1);
        clip_positions[2] = shader.vertex(0, 2);

        TriangleRasterizer::drawTriangle(clip_positions, shader, frame_buffer);
    }
}


// ---------------------------------------------------------------------------
// Varyings: written by vertex() per corner, read back by fragment() blended.
// ---------------------------------------------------------------------------

// Gouraud's varying is one float intensity per corner. With the light on +z each
// intensity is its normal's z exactly, so the corners light at 1, 0.5 and 0.25 and
// every channel below is a whole number.
TEST_CASE("a varying evaluated at a corner returns that corner's value exactly", "[shading][varying]")
{
    const Mesh mesh = makeTriangleMesh({0.0f, 0.0f, 1.0f}, {0.0f, 0.8660254f, 0.5f}, {0.0f, 0.9682458f, 0.25f});
    GouraudShader shader(mesh, tinymath::Matrix4x4{}, {0.0f, 0.0f, 1.0f}, BASE);

    REQUIRE(static_cast<int>(shade(shader, AT_A).r) == 200);
    REQUIRE(static_cast<int>(shade(shader, AT_B).r) == 100);
    REQUIRE(static_cast<int>(shade(shader, AT_C).r) == 50);
}


TEST_CASE("a varying at the centroid returns the mean of the three corners", "[shading][varying]")
{
    const Mesh mesh = makeTriangleMesh({0.0f, 0.0f, 1.0f}, {0.0f, 0.8660254f, 0.5f}, {0.0f, 0.9682458f, 0.25f});
    GouraudShader shader(mesh, tinymath::Matrix4x4{}, {0.0f, 0.0f, 1.0f}, BASE);

    const float expected = 200.0f * (1.0f + 0.5f + 0.25f) / 3.0f;
    REQUIRE(static_cast<int>(shade(shader, CENTROID).r) == Catch::Approx(expected).margin(CHANNEL_MARGIN));
}


// ---------------------------------------------------------------------------
// The shaders, one property each.
// ---------------------------------------------------------------------------

// FaceShader computes its normal from the cross product of the face's own edges. The
// file's vn are set facing away from the light on purpose: a shader reading them
// would render the face black.
TEST_CASE("FaceShader agrees with a hand-computed max(0, n.l)", "[shading][face]")
{
    const tinymath::Vec3f away{0.0f, 0.0f, -1.0f};
    const Mesh mesh = makeTriangleMesh(away, away, away);

    SECTION("light at 45 degrees to the face normal gives cos(45)")
    {
        FaceShader shader(mesh, tinymath::Matrix4x4{}, {0.0f, 1.0f, 1.0f}, BASE);
        const float expected = 200.0f * std::sqrt(0.5f);
        REQUIRE(static_cast<int>(shade(shader, CENTROID).r) == Catch::Approx(expected).margin(CHANNEL_MARGIN));
    }

    SECTION("light behind the face clamps to zero")
    {
        FaceShader shader(mesh, tinymath::Matrix4x4{}, {0.0f, 0.0f, -1.0f}, BASE);
        REQUIRE(static_cast<int>(shade(shader, CENTROID).r) == 0);
    }

    SECTION("the weights are ignored: one intensity for the whole face")
    {
        FaceShader shader(mesh, tinymath::Matrix4x4{}, {0.0f, 1.0f, 1.0f}, BASE);
        const Color at_a = shade(shader, AT_A);
        REQUIRE(shade(shader, AT_B) == at_a);
        REQUIRE(shade(shader, CENTROID) == at_a);
    }
}


// A barycentric blend of differing unit normals lands inside the unit sphere. The
// three axes blend at the centroid to {1/3, 1/3, 1/3}, length 1/sqrt(3) ~ 0.577.
TEST_CASE("a blended normal is not unit length, which is why LambertShader re-normalizes", "[shading][lambert]")
{
    const tinymath::Vec3f x_axis{1.0f, 0.0f, 0.0f};
    const tinymath::Vec3f y_axis{0.0f, 1.0f, 0.0f};
    const tinymath::Vec3f z_axis{0.0f, 0.0f, 1.0f};

    const tinymath::Vec3f blended = x_axis * CENTROID.alpha + y_axis * CENTROID.beta + z_axis * CENTROID.gamma;
    REQUIRE(tinymath::length(blended) == Catch::Approx(1.0f / std::sqrt(3.0f)));

    // The light points exactly along the re-normalized blend, so the pixel is fully
    // lit. Skipping the normalize would leave n.l = 0.577 and a channel of ~115.
    const Mesh mesh = makeTriangleMesh(x_axis, y_axis, z_axis);
    LambertShader shader(mesh, tinymath::Matrix4x4{}, {1.0f, 1.0f, 1.0f}, BASE);
    REQUIRE(static_cast<int>(shade(shader, CENTROID).r) == Catch::Approx(200.0f).margin(CHANNEL_MARGIN));
}


// N on +z, L at +45 degrees and V at -45 degrees in the xz plane: the reflected light
// lands exactly on the eye (r = v) and the half vector exactly on the normal (h = n).
// That is the mirror orientation, where both lobes peak at 1. The base colour is black
// and ambient 0, so the channel is the specular term alone.
TEST_CASE("Phong and Blinn-Phong both give full specular at the mirror orientation", "[shading][specular]")
{
    const tinymath::Vec3f normal{0.0f, 0.0f, 1.0f};
    const Mesh mesh = makeTriangleMesh(normal, normal, normal);

    PhongShader phong(mesh, tinymath::Matrix4x4{}, fromDegrees(45.0f), fromDegrees(-45.0f), BLACK_BASE, BASE, 0.0f, 64.0f);
    BlinnPhongShader blinn_phong(mesh, tinymath::Matrix4x4{}, fromDegrees(45.0f), fromDegrees(-45.0f), BLACK_BASE, BASE, 0.0f, 64.0f);

    REQUIRE(static_cast<int>(shade(phong, CENTROID).r) == Catch::Approx(200.0f).margin(CHANNEL_MARGIN));
    REQUIRE(static_cast<int>(shade(blinn_phong, CENTROID).r) == Catch::Approx(200.0f).margin(CHANNEL_MARGIN));
}


// For coplanar n, l and v the angle between n and h is exactly half the angle between
// r and v. Here l is 60 degrees off n and v is 20 degrees off on the other side, so r
// sits at -60 (40 degrees from v) while h sits at +20 (20 degrees from n). A lobe's
// width goes like 1/sqrt(exponent), so halving the angle is why Blinn-Phong needs
// about 4x Phong's exponent to draw the same highlight.
TEST_CASE("the half vector's angle to n is half the reflection's angle to v", "[shading][specular]")
{
    const tinymath::Vec3f normal{0.0f, 0.0f, 1.0f};
    const tinymath::Vec3f light = fromDegrees(60.0f);
    const tinymath::Vec3f view = fromDegrees(-20.0f);

    const tinymath::Vec3f reflection = normal * (2.0f * tinymath::dot(normal, light)) - light;
    const tinymath::Vec3f half_vector = tinymath::normalize(light + view);

    const float reflection_angle = std::acos(tinymath::dot(reflection, view));
    const float half_vector_angle = std::acos(tinymath::dot(normal, half_vector));

    REQUIRE(reflection_angle == Catch::Approx(40.0f * PI / 180.0f).margin(1e-4));
    REQUIRE(half_vector_angle == Catch::Approx(reflection_angle / 2.0f).margin(1e-4));

    // The same geometry through the shaders: at equal exponent Blinn-Phong is brighter
    // off the peak, cos(20)^4 ~ 0.78 against Phong's cos(40)^4 ~ 0.34.
    const Mesh mesh = makeTriangleMesh(normal, normal, normal);
    PhongShader phong(mesh, tinymath::Matrix4x4{}, light, view, BLACK_BASE, BASE, 0.0f, 4.0f);
    BlinnPhongShader blinn_phong(mesh, tinymath::Matrix4x4{}, light, view, BLACK_BASE, BASE, 0.0f, 4.0f);

    REQUIRE(static_cast<int>(shade(blinn_phong, CENTROID).r) > static_cast<int>(shade(phong, CENTROID).r));
}


// ---------------------------------------------------------------------------
// The rasterizer's side of the contract.
// ---------------------------------------------------------------------------

// Colour alone is not enough: if a discarded fragment still wrote depth, the invisible
// triangle would silently occlude everything drawn behind it afterwards.
TEST_CASE("fragment() returning false leaves both the colour and the depth untouched", "[shading][discard]")
{
    Framebuffer fb(16, 16);
    fb.clear(BLACK);

    ConstantShader far_blue(0.25f, BLUE, true);
    drawWithShader(far_blue, fb);

    ConstantShader near_discarded(0.75f, RED, false);
    drawWithShader(near_discarded, fb);

    REQUIRE(fb.getPixel(5, 5) == BLUE);
    REQUIRE(fb.getDepth(5, 5) == Catch::Approx(0.25f));

    // Behind the discarded triangle, in front of the blue one: it must still win.
    ConstantShader middle_red(0.5f, RED, true);
    drawWithShader(middle_red, fb);

    REQUIRE(fb.getPixel(5, 5) == RED);
    REQUIRE(fb.getDepth(5, 5) == Catch::Approx(0.5f));
}
