#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "math/Vec3.h"

TEST_CASE("Vec3 initialization and templating check", "[Vec3]")
{
    tinymath::Vec3<int> vec3_int{10, 20, 30};
    REQUIRE(vec3_int.x == 10);
    REQUIRE(vec3_int.y == 20);
    REQUIRE(vec3_int.z == 30);

    // Values chosen to be exactly representable, so == is safe.
    tinymath::Vec3f vec3_float{0.5f, -1.75f, 2.25f};
    REQUIRE(vec3_float.x == 0.5f);
    REQUIRE(vec3_float.y == -1.75f);
    REQUIRE(vec3_float.z == 2.25f);
}


TEST_CASE("Vec3 addition", "[Vec3]")
{
    tinymath::Vec3<int> a{10, 20, 30};
    tinymath::Vec3<int> b{3, 8, -5};

    tinymath::Vec3<int> sum = a + b;
    REQUIRE(sum.x == 13);
    REQUIRE(sum.y == 28);
    REQUIRE(sum.z == 25);
}


TEST_CASE("Vec3 subtraction", "[Vec3]")
{
    tinymath::Vec3<int> a{10, 20, 30};
    tinymath::Vec3<int> b{3, 8, -5};

    tinymath::Vec3<int> diff = a - b;
    REQUIRE(diff.x == 7);
    REQUIRE(diff.y == 12);
    REQUIRE(diff.z == 35);
}


TEST_CASE("Vec3 scalar multiplication", "[Vec3]")
{
    tinymath::Vec3<int> a{2, -3, 4};

    tinymath::Vec3<int> scaled = a * 5;
    REQUIRE(scaled.x == 10);
    REQUIRE(scaled.y == -15);
    REQUIRE(scaled.z == 20);

    // Scaling by zero collapses to the origin.
    tinymath::Vec3<int> collapsed = a * 0;
    REQUIRE(collapsed.x == 0);
    REQUIRE(collapsed.y == 0);
    REQUIRE(collapsed.z == 0);

    // Callable on a const object (operator* is const).
    tinymath::Vec3f const b{0.5f, -1.5f, 2.0f};
    tinymath::Vec3f scaled_float = b * 2.0f;
    REQUIRE(scaled_float.x == 1.0f);
    REQUIRE(scaled_float.y == -3.0f);
    REQUIRE(scaled_float.z == 4.0f);
}


TEST_CASE("Vec3 dot is the sum of component products", "[Vec3]")
{
    // Known value: 1*4 + 2*5 + 3*6 = 32.
    REQUIRE(dot(tinymath::Vec3<int>{1, 2, 3}, tinymath::Vec3<int>{4, 5, 6}) == 32);

    // Perpendicular vectors have zero dot product.
    REQUIRE(dot(tinymath::Vec3<int>{1, 0, 0}, tinymath::Vec3<int>{0, 1, 0}) == 0);

    // Dot with itself is the squared length: 2*2 + 3*3 + 6*6 = 49.
    REQUIRE(dot(tinymath::Vec3<int>{2, 3, 6}, tinymath::Vec3<int>{2, 3, 6}) == 49);

    // Order does not matter (commutative).
    REQUIRE(dot(tinymath::Vec3<int>{1, 2, 3}, tinymath::Vec3<int>{4, 5, 6})
         == dot(tinymath::Vec3<int>{4, 5, 6}, tinymath::Vec3<int>{1, 2, 3}));
}


TEST_CASE("Vec3 cross produces the perpendicular vector", "[Vec3]")
{
    // Right-handed basis: +x cross +y = +z.
    tinymath::Vec3<int> basis = cross(tinymath::Vec3<int>{1, 0, 0}, tinymath::Vec3<int>{0, 1, 0});
    REQUIRE(basis.x == 0);
    REQUIRE(basis.y == 0);
    REQUIRE(basis.z == 1);

    // General case — every term is non-zero, so a swapped or dropped
    // factor in any component cannot hide behind a zero.
    // (2*6-3*5, 3*4-1*6, 1*5-2*4) = (-3, 6, -3)
    tinymath::Vec3<int> const a{1, 2, 3};
    tinymath::Vec3<int> const b{4, 5, 6};

    tinymath::Vec3<int> product = cross(a, b);
    REQUIRE(product.x == -3);
    REQUIRE(product.y == 6);
    REQUIRE(product.z == -3);

    // Anti-commutative: swapping the arguments negates every component.
    tinymath::Vec3<int> reversed = cross(b, a);
    REQUIRE(reversed.x == -product.x);
    REQUIRE(reversed.y == -product.y);
    REQUIRE(reversed.z == -product.z);

    // The result is perpendicular to both operands.
    REQUIRE(dot(product, a) == 0);
    REQUIRE(dot(product, b) == 0);

    // Parallel vectors span no area — cross is the zero vector.
    tinymath::Vec3<int> parallel = cross(tinymath::Vec3<int>{1, 2, 3}, tinymath::Vec3<int>{2, 4, 6});
    REQUIRE(parallel.x == 0);
    REQUIRE(parallel.y == 0);
    REQUIRE(parallel.z == 0);

    // A vector crossed with itself is also zero.
    tinymath::Vec3<int> self = cross(a, a);
    REQUIRE(self.x == 0);
    REQUIRE(self.y == 0);
    REQUIRE(self.z == 0);
}


TEST_CASE("Vec3 length is the euclidean magnitude", "[Vec3]")
{
    // Pythagorean triples: the sum of squares is a perfect square, so the
    // square root is exact in float and == is safe.
    REQUIRE(length(tinymath::Vec3f{3.0f, 4.0f, 0.0f}) == 5.0f);
    REQUIRE(length(tinymath::Vec3f{2.0f, 3.0f, 6.0f}) == 7.0f);

    // A unit axis has length 1.
    REQUIRE(length(tinymath::Vec3f{0.0f, 1.0f, 0.0f}) == 1.0f);

    // Length is unsigned — negating every component cannot change it.
    REQUIRE(length(tinymath::Vec3f{-3.0f, -4.0f, 0.0f}) == 5.0f);

    // The zero vector is the only one with zero length. It is also the input
    // normalize cannot survive — see the note at the end of the next case.
    REQUIRE(length(tinymath::Vec3f{0.0f, 0.0f, 0.0f}) == 0.0f);
}


TEST_CASE("Vec3 normalize preserves direction and yields unit length", "[Vec3]")
{
    // Hand-computable: the length is exactly 7, so the components must be
    // 2/7, 3/7, 6/7. Pinning all three catches a swapped or dropped component,
    // which a length-only check would pass.
    tinymath::Vec3f const a{2.0f, 3.0f, 6.0f};

    tinymath::Vec3f unit = normalize(a);
    REQUIRE(unit.x == Catch::Approx(2.0f / 7.0f));
    REQUIRE(unit.y == Catch::Approx(3.0f / 7.0f));
    REQUIRE(unit.z == Catch::Approx(6.0f / 7.0f));

    // The defining property, on a vector with no tidy length.
    REQUIRE(length(normalize(tinymath::Vec3f{1.0f, 2.0f, 3.0f})) == Catch::Approx(1.0f));

    // Sign survives. An abs() slipped into length or normalize would turn this
    // into +1 and still pass every magnitude assertion above.
    // 1/4 is exactly representable, so this needs no tolerance.
    tinymath::Vec3f negative = normalize(tinymath::Vec3f{0.0f, 0.0f, -4.0f});
    REQUIRE(negative.x == 0.0f);
    REQUIRE(negative.y == 0.0f);
    REQUIRE(negative.z == -1.0f);

    // Direction is scale-invariant: normalize discards magnitude and nothing
    // else, so a vector and its tenfold produce the same unit vector.
    tinymath::Vec3f base = normalize(tinymath::Vec3f{1.0f, 2.0f, 3.0f});
    tinymath::Vec3f scaled_up = normalize(tinymath::Vec3f{10.0f, 20.0f, 30.0f});
    REQUIRE(base.x == Catch::Approx(scaled_up.x));
    REQUIRE(base.y == Catch::Approx(scaled_up.y));
    REQUIRE(base.z == Catch::Approx(scaled_up.z));

    // Normalizing an already-unit vector is a no-op — the divide is by 1.
    tinymath::Vec3f axis = normalize(tinymath::Vec3f{0.0f, 1.0f, 0.0f});
    REQUIRE(axis.x == 0.0f);
    REQUIRE(axis.y == 1.0f);
    REQUIRE(axis.z == 0.0f);

    // normalize({0, 0, 0}) is deliberately NOT tested. It is the open question
    // of this lesson: unguarded, it divides by zero and returns all-NaN. A test
    // here would pin down behaviour we intend to change once lookAt shows what
    // that failure actually looks like on screen.
}
