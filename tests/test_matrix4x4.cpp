#include <catch2/catch_test_macros.hpp>

#include "math/Matrix4x4.h"

namespace
{
    // Uniform scale by 2. Diagonal only.
    tinymath::Matrix4x4 makeScale()
    {
        tinymath::Matrix4x4 scale;
        scale.data[0][0] = 2.0f;
        scale.data[1][1] = 2.0f;
        scale.data[2][2] = 2.0f;
        return scale;
    }

    // Translate by (2, 3, 4). Column-vector convention puts the offset in the
    // fourth column, not the bottom row.
    tinymath::Matrix4x4 makeTranslate()
    {
        tinymath::Matrix4x4 translate;
        translate.data[0][3] = 2.0f;
        translate.data[1][3] = 3.0f;
        translate.data[2][3] = 4.0f;
        return translate;
    }
}


TEST_CASE("Matrix4x4 default constructs to identity", "[Matrix4x4]")
{
    tinymath::Matrix4x4 const identity;

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            REQUIRE(identity.data[i][j] == (i == j ? 1.0f : 0.0f));
        }
    }
}


TEST_CASE("Identity leaves a vector unchanged", "[Matrix4x4]")
{
    tinymath::Matrix4x4 const identity;
    tinymath::Vec4f const v{0.5f, -1.75f, 2.25f, 1.0f};

    tinymath::Vec4f result = identity * v;
    REQUIRE(result.x == v.x);
    REQUIRE(result.y == v.y);
    REQUIRE(result.z == v.z);
    REQUIRE(result.w == v.w);
}


TEST_CASE("Identity is neutral under matrix multiplication", "[Matrix4x4]")
{
    tinymath::Matrix4x4 const identity;

    // Asymmetric on purpose: a symmetric matrix survives a transposed
    // implementation, so it would prove nothing here.
    tinymath::Matrix4x4 asymmetric = makeTranslate();
    asymmetric.data[0][1] = 5.0f;
    asymmetric.data[2][0] = -7.0f;

    tinymath::Matrix4x4 left = identity * asymmetric;
    tinymath::Matrix4x4 right = asymmetric * identity;

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            REQUIRE(left.data[i][j] == asymmetric.data[i][j]);
            REQUIRE(right.data[i][j] == asymmetric.data[i][j]);
        }
    }
}


TEST_CASE("Translation acts on points but not on directions", "[Matrix4x4]")
{
    tinymath::Matrix4x4 const translate = makeTranslate();

    // w == 1 is a position, so the fourth column reaches it.
    tinymath::Vec4f point = translate * tinymath::Vec4f{1.0f, 1.0f, 1.0f, 1.0f};
    REQUIRE(point.x == 3.0f);
    REQUIRE(point.y == 4.0f);
    REQUIRE(point.z == 5.0f);
    REQUIRE(point.w == 1.0f);

    // w == 0 is a direction. It has no location, so translating it is a no-op
    // — which is the whole reason the homogeneous coordinate exists.
    tinymath::Vec4f direction = translate * tinymath::Vec4f{1.0f, 1.0f, 1.0f, 0.0f};
    REQUIRE(direction.x == 1.0f);
    REQUIRE(direction.y == 1.0f);
    REQUIRE(direction.z == 1.0f);
    REQUIRE(direction.w == 0.0f);
}


TEST_CASE("Matrix multiplication is not commutative", "[Matrix4x4]")
{
    tinymath::Matrix4x4 const scale = makeScale();
    tinymath::Matrix4x4 const translate = makeTranslate();

    tinymath::Matrix4x4 translate_then_scale = scale * translate;
    tinymath::Matrix4x4 scale_then_translate = translate * scale;

    // The translation column differs: scaling after translating scales the
    // offset too (2,3,4 -> 4,6,8), scaling before leaves it alone.
    REQUIRE(translate_then_scale.data[0][3] == 4.0f);
    REQUIRE(translate_then_scale.data[1][3] == 6.0f);
    REQUIRE(translate_then_scale.data[2][3] == 8.0f);

    REQUIRE(scale_then_translate.data[0][3] == 2.0f);
    REQUIRE(scale_then_translate.data[1][3] == 3.0f);
    REQUIRE(scale_then_translate.data[2][3] == 4.0f);
}


TEST_CASE("Composition applies the rightmost matrix first", "[Matrix4x4]")
{
    tinymath::Matrix4x4 const scale = makeScale();
    tinymath::Matrix4x4 const translate = makeTranslate();
    tinymath::Vec4f const point{1.0f, 1.0f, 1.0f, 1.0f};

    // Column-vector convention: in (translate * scale), scale runs first.
    // (1,1,1) -> scaled (2,2,2) -> translated (4,5,6).
    tinymath::Vec4f scale_first = (translate * scale) * point;
    REQUIRE(scale_first.x == 4.0f);
    REQUIRE(scale_first.y == 5.0f);
    REQUIRE(scale_first.z == 6.0f);
    REQUIRE(scale_first.w == 1.0f);

    // Reversed: translate runs first. (1,1,1) -> (3,4,5) -> scaled (6,8,10).
    tinymath::Vec4f translate_first = (scale * translate) * point;
    REQUIRE(translate_first.x == 6.0f);
    REQUIRE(translate_first.y == 8.0f);
    REQUIRE(translate_first.z == 10.0f);
    REQUIRE(translate_first.w == 1.0f);

    // Composing then applying must equal applying one at a time. This is the
    // property step 5 relies on when the whole pipeline collapses into one
    // matrix built once per frame.
    tinymath::Vec4f applied_separately = translate * (scale * point);
    REQUIRE(applied_separately.x == scale_first.x);
    REQUIRE(applied_separately.y == scale_first.y);
    REQUIRE(applied_separately.z == scale_first.z);
    REQUIRE(applied_separately.w == scale_first.w);
}


TEST_CASE("Matrix multiplication pairs rows with columns", "[Matrix4x4]")
{
    // Every entry non-zero and distinct, so a swapped index cannot hide behind
    // a zero or a coincidence.
    tinymath::Matrix4x4 a;
    tinymath::Matrix4x4 b;

    float value = 1.0f;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            a.data[i][j] = value;
            b.data[i][j] = value * 2.0f;
            value += 1.0f;
        }
    }

    tinymath::Matrix4x4 product = a * b;

    // result[0][0] = row 0 of a . column 0 of b
    //              = 1*2 + 2*10 + 3*18 + 4*26 = 180
    REQUIRE(product.data[0][0] == 180.0f);

    // result[0][1] = row 0 of a . column 1 of b
    //              = 1*4 + 2*12 + 3*20 + 4*28 = 200
    REQUIRE(product.data[0][1] == 200.0f);

    // result[3][0] = row 3 of a . column 0 of b
    //              = 13*2 + 14*10 + 15*18 + 16*26 = 852
    REQUIRE(product.data[3][0] == 852.0f);

    // result[3][3] = row 3 of a . column 3 of b
    //              = 13*8 + 14*16 + 15*24 + 16*32 = 1200
    REQUIRE(product.data[3][3] == 1200.0f);
}
