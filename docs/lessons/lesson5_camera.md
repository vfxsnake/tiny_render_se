# Lesson 5 — Better camera

**Source:** https://haqr.eu/tinyrenderer/camera/

> **Status:** implementation plan agreed, no code written. Written ahead of the spike by explicit request — the
> usual "spike first, document second" order was skipped because Lesson 4 already established every concept this
> lesson builds on, and the design fork (`Matrix4x4`) was settled at the end of Lesson 4 rather than being open.

## Goal

Replace the hand-composed chain `orthographicProjection(perspectiveZDivide(rotateY(v)))` with a single composed
matrix, `Viewport · Perspective · ModelView · v`, so the camera becomes a position-and-target the caller states
rather than an angle and an eye distance baked into the vertex loop.

## Exit condition

The model renders identically to the Lesson 4 spike — rotated off-axis, near side larger, depth-correct — but
driven entirely through `Matrix4x4`, with the transform built **once per frame** instead of three function calls
per vertex. `rotateY` and `perspectiveZDivide` are unreferenced by the render path.

The pixel-identical comparison against the spike is the regression check, and it is only available *before* the
spike functions are deleted. Do it then.

## Concepts

### Linear vs affine, and why 4×4

A 3×3 matrix expresses every *linear* map — rotation, scale, shear, reflection. What it cannot express is
**translation**, and that is a difference of kind rather than of size: a linear map must fix the origin, and
translation by definition does not. An affine map is `T(x) = Ax + b`.

Adding a fourth coordinate and carrying points as `(x, y, z, 1)` makes translation linear one dimension up — the
translation column simply multiplies the constant `1`. This is why `Matrix3x3` was cancelled at the end of
Lesson 4 rather than built: the 3×3 covers only the rotation half of what this lesson needs.

### Homogeneous coordinates and the perspective divide

Any scalar multiple `(wx, wy, wz, w)` denotes the same point, recovered by dividing through by `w`. That is the
second thing the fourth dimension buys, and it is the more important one: a term in the **bottom row** makes `w`
a function of `z`, so the divide by `w` *is* the near-things-look-bigger division.

Lesson 4 hand-rolled that division as `k = 1/(1 − z/c)` in `perspectiveZDivide`. Here it collapses into the
matrix pipeline alongside everything else, and the arc closes: the same arithmetic, now expressed as data rather
than as a function.

### The five spaces

`Viewport( Perspective( ModelView( v ) ) )` — right-to-left under the column-vector convention. Each matrix is a
change of coordinate system, and the composition is one matrix multiply per stage done **once**, then one
matrix-vector multiply per vertex.

### The camera frame

`lookAt` does not take an angle. It takes where the eye is, what it is looking at, and which way is roughly up,
then builds an orthonormal basis from those by cross products:

- `n = normalize(eye − center)` — the camera's forward axis (pointing *back* toward the eye)
- `l = normalize(cross(up, n))` — the camera's right axis
- `m = normalize(cross(n, l))` — the camera's true up, re-derived so the frame is orthonormal even when the
  supplied `up` was only approximate

The `up` argument being *approximate* is the point: the caller states an intent ("y is up") and the cross
products repair it into an exact basis.

### Where the model-view / projection split shows up

Lesson 4 put `rotateY` in `Transform.h` and `perspectiveZDivide` in `Projection.h`, and recorded that the file
boundary *is* the model-view / projection separation. This lesson pays that off without rearranging anything:
`lookAt` joins `Transform.h`, `perspective` and `viewport` join `Projection.h`.

## Design decisions

| Decision | Choice | Reason |
|----------|--------|--------|
| Matrix size | `Matrix4x4` — `Matrix3x3` cancelled | 3×3 cannot express translation *or* perspective; both are needed here (decided end of Lesson 4) |
| Vector convention | **Column vectors**, `M * v`, compose right-to-left | Matches the lesson, every graphics API, and every reference the user will read next; effectively unchangeable later |
| Storage order | Row-major | CLAUDE.md; independent of the column-vector convention above |
| `Matrix4x4` location | `src/math/Matrix4x4.h`, header-only | CLAUDE.md keeps math types header-only |
| `Vec4` shape | Templated aggregate + `Vec4f` alias, mirroring `Vec3.h` | Consistency; `Vec3` has no constructors, so `Vec4` must not either or brace-init diverges between them |
| Vec3 ↔ Vec4 conversion | Free functions, not constructors | Keeps `Vec4` an aggregate (see above). Names are the user's call |
| `lookAt` location | `Transform.h` | Model-view side of the split established in Lesson 4 |
| `perspective` / `viewport` location | `Projection.h` | Projection side of the same split; `viewport` supersedes `orthographicProjection` |
| Viewport depth row | **Deviates from the lesson** — scale and translate `z` by `d/2`, do not pass it through | Our depth test convention requires it; see the warning below |
| `w` retention | Not retained per-vertex yet | Needed only for perspective-correct interpolation, which first bites at Shading |
| Test timing | Tests **before** use, for both new math types | Standing rule: math types get tests first; visual algorithms get them last |

### ⚠️ The viewport depth row

The lesson's viewport matrix passes `z` through unchanged — its third row is `[0 0 1 0]`. **Ours must not.**

`orthographicProjection` maps `z: [−1,1] → [0,1]`, and the depth test depends on that mapping: `Framebuffer`
clears to `0.0f` and keeps the larger value, so any fragment arriving with depth ≤ 0 loses to the clear and
punches a hole. Copying the lesson's row verbatim sends every vertex with `z < 0` — half the model — below the
clear value.

The fix is to put `d/2` in both the scale and the translate slot of that row, reproducing the existing mapping.

This is also the moment to close a carry-forward that has been open since Lesson 3: the bare `0.0f` at
`Framebuffer.cpp:9`, `:45` and `:79` is an undocumented convention that this lesson makes load-bearing. Give it a
name and a comment while the reason is fresh.

## Implementation order

1. **`Vec4`** — type, conversions, tests.
2. **`Matrix4x4`** — type, multiply, tests.
3. **`Vec3` gains `length()` / `normalize()`** — free functions, as CLAUDE.md already specifies. `lookAt` is the
   caller that finally forces them; only `dot` and `cross` exist today.
4. **The three builders** — `lookAt`, `perspective`, `viewport`.
5. **Wire the matrix path into its own draw function** — compose once outside the triangle loop; per vertex
   embed → multiply → divide by `w` → build the `Triangle`.
6. **Compare the two paths** — confirm the matrix path renders pixel-identical to the spike.

**Step 6 amended in Session 34.** It was originally *"retire the spike — after confirming the image is
unchanged"*. The spike functions are now **kept**: the matrix path goes in **beside** `testDrawMesh` as a
second draw function rather than replacing its body, so the two pipelines can be rendered against each other on
demand. This is a learning project, and the superseded rung is worth keeping as the reference the new one is
checked against — the same treatment the three line-drawing rungs and the two triangle-rasterization rungs
already receive. The pixel-identical check therefore stops being a one-shot gate before a deletion and becomes a
standing A/B.

Step 5 also disposes of a Lesson 4 note without further work: `rotateY` recomputed `std::cos`/`std::sin` per
vertex, and the trig now runs once inside `lookAt`. That was the benchmark that would have justified
`Matrix3x3`; the 4×4 answers it by construction.

## Modules

### `src/math/Vec4.h`

**Responsibility:** homogeneous 4-component vector; the type the matrix pipeline transports points in.

**API:**
- `template <typename T> struct Vec4 { T x, y, z, w; }` — aggregate, no constructors
- `Vec4<T> operator+(const Vec4<T>&) const`
- `Vec4<T> operator-(const Vec4<T>&) const`
- `Vec4<T> operator*(const T) const`
- `Vec4f <embed>(Vec3f)` — free function; sets `w = 1`
- `Vec3f <divide>(Vec4f)` — free function; divides x, y, z by `w`. **This is the perspective divide.**
- `using Vec4f = Vec4<float>`

Conversion function names deliberately left open — the user's call.

### `src/math/Matrix4x4.h`

**Responsibility:** 4×4 float matrix, row-major storage, column-vector convention.

**API:**
- `Matrix4x4 identity()` — free function or static member; user's call
- `Matrix4x4 operator*(const Matrix4x4&) const`
- `Vec4f operator*(const Vec4f&) const`
- element access

### `src/math/Vec3.h` (extended)

**API added:**
- `template <typename T> T length(Vec3<T>)`
- `template <typename T> Vec3<T> normalize(Vec3<T>)`

### `src/math/Transform.h/.cpp` (extended)

**API added:**
- `Matrix4x4 lookAt(Vec3f eye, Vec3f center, Vec3f up)` — orthonormal camera basis × translate by `−center`

**Kept, not removed** *(amended Session 34)*: `rotateY` stays — both as the spike half of the standing A/B, and
because it remains the model transform (the M that `lookAt` does not supply).

### `src/math/Projection.h/.cpp` (extended)

**API added:**
- `Matrix4x4 perspective(float focal_length)` — identity with `−1/f` in the bottom row
- `Matrix4x4 viewport(int screen_width, int screen_height)` — clip `[−1,1]` → screen, **with our depth mapping**

  *Amended in Session 34.* The originally planned `(int x, int y, int width, int height)` signature carried a
  viewport **origin offset** for rendering into a sub-rectangle. Dropped — nothing calls with a non-zero origin,
  same rule that cancelled `Matrix3x3` and `length2`. The depth parameter is likewise absent: `depth_scale` was
  `1.0f` at every one of the three `orthographicProjection` call sites, so the `[−1,1] → [0,1]` mapping is a
  constant in the body, not a parameter.

**Kept, not removed** *(amended Session 34)*: `perspectiveZDivide` and `orthographicProjection` both stay as the
spike half of the standing A/B against `perspective` / `viewport`.

### `tests/test_vec4.cpp`

**Cases:**
- aggregate init and field order
- `+`, `-`, `*scalar`
- embed sets `w = 1` and leaves x, y, z untouched
- divide by `w` with `w == 1` is the identity on x, y, z
- divide by `w` with `w != 1` actually scales — the case that catches an embed/divide mix-up

### `tests/test_matrix4x4.cpp`

**Cases:**
- identity is neutral for `M * M` and for `M * v`
- multiplication is associative
- multiplication is **not** commutative — pins the column-vector convention, and fails loudly if the operand
  order is ever silently swapped
- a known rotation against hand-computed values
- a translation moves a point but leaves a direction (`w = 0`) unmoved — the single test that proves the
  homogeneous coordinate is doing its job

## Carry-forwards to fold in

- **Compiler warning level** — `/W4` (MSVC) / `-Wall -Wextra` in `CMakeLists.txt`, still unset, now four lessons
  overdue. Step 2 is a large block of new index arithmetic to write with warnings off.
- **`Framebuffer` depth convention** — see the viewport warning above. Named constant plus a comment at
  `Framebuffer.cpp:9`, `:45`, `:79`.
