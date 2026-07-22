# Lesson 2 — Triangle Rasterization

**Source:** https://haqr.eu/tinyrenderer/rasterization/

## Goal
Fill a triangle with solid color so that flat shapes — and ultimately the OBJ head — read as surfaces instead of wireframes.

## Exit condition
The window shows at least one solid-filled triangle drawn from three hardcoded `Vec2i` points, and `tests/test_triangle_rasterizer.cpp` passes — including the differential case proving the scanline and bounding-box rungs fill a bit-identical pixel set.

## Concepts

### The two fill strategies (the naive-to-optimised ladder)
- **Scanline (rung 1, "old-school"):** sort the three vertices by `y`, split the triangle at the middle vertex into a flat-bottom and a flat-top half, and sweep horizontal spans between two edges row by row. Its weakness is not floats — it is the *serial* edge-tracking bookkeeping and per-row branches (a loop-carried dependency, the same class of problem as Lesson 1's `accum` rung). We build it to feel that pain.
- **Bounding box + barycentric (rung 2):** compute the triangle's bounding box, then for every pixel in the box test whether it is inside via barycentric coordinates. Every pixel is independent — no bookkeeping carried between iterations — so it is parallel-friendly (the exact vectorization lesson from Lesson 1). This is the strategy that maps to the GPU in Phase 2.

### Barycentric coordinates
Any point `P` in the plane of triangle `ABC` can be written `P = αA + βB + γC` with weights `(α, β, γ)` that **always sum to 1** (this is their definition, true everywhere in the plane — not a test).
- At vertex `A`: `(1, 0, 0)`.
- At the centroid: `(⅓, ⅓, ⅓)` — pulled equally by all three vertices.
- **Inside test: all three weights ≥ 0.** That is the entire test. "Between 0 and 1" follows for free (if all ≥ 0 and they sum to 1, none can exceed 1); "sum to 1" is always true and tests nothing.
- **Edge convention:** on edge `BC`, `α = 0` exactly. With `≥ 0` that pixel is **drawn** (we fill the boundary). Trade-off: a pixel on an edge shared by two triangles is drawn by both — harmless for opaque flat fills; only matters with blending / strict fill rules (revisit in later lessons).

### Signed area / back-face culling
Barycentric weights are ratios of sub-triangle areas to the whole triangle's area. The area comes from a **cross product** of two edge vectors, which is **signed**: its sign flips with the vertex winding (CW vs CCW on screen). That sign encodes **facing** — CCW faces the viewer, CW faces away — so `if (signed_area < 0) skip;` is back-face culling for free, reusing the number we already need for the weights.
- **But** facing is only meaningful once triangles come from a mesh with a *consistent* winding convention. For a single hardcoded 2D triangle there is no meaningful front/back, and a naive cull would make a "wrong-wound" triangle vanish.
- **Policy this lesson:** take `abs()` of the signed area for the inside test so the triangle draws regardless of winding; leave a comment marking where the real cull goes. Back-face culling is wired for real in Lesson 3 (OBJ head). Same build-the-hook / defer-the-payoff pattern as Phase 0.5.

### New primitives (discovery-driven)
Smallest visible thing: **one filled triangle**. Minimum data to define it: three points — we already have `Vec2i`. So the only genuinely new *code* is the fill function plus a small `Triangle` data object. Everything 3D is deferred until the head demands it: **no `Vec3`, no OBJ loader, no projection/matrices** this lesson.

## Design decisions
| Decision | Choice | Reason |
|----------|--------|--------|
| Module shape | `TriangleRasterizer` namespace with free functions | No state — same call as `LineDrawer`; no class needed |
| Rungs | Build **both** scanline + bbox/barycentric, benchmark both | Mirror Lesson 1's ladder; measure the real winner on this hardware |
| Winner naming | Name by strategy now (`drawTriangleScanline` / `drawTriangleBbox`); winner renamed `drawTriangle` after benchmarking | Same close-out as Lesson 1; don't pick a winner before measuring |
| Triangle representation | `struct Triangle { Vec2i a, b, c; }` — data object, no methods | A domain object that holds points; performs no arithmetic on itself, so it is not a math type |
| `Triangle` location | `src/rasterizer/primitives/Triangle.h` | It is a rasterizer primitive, not linear algebra; transforms-as-components may live alongside later |
| `Vec2` location | Stays in `src/math/` | Pure math operand — gets `cross`/`dot` this and later lessons; it *is* algebra |
| Signed-area source | Free `cross(Vec2, Vec2)` → scalar, added to `Vec2.h` | 2D cross is a scalar = signed area; the barycentric + cull building block |
| Winding policy | `abs()` of signed area for the inside test; cull hook comment-marked | One hardcoded triangle must draw regardless of winding; real cull deferred to Lesson 3 |
| Tests | Written at lesson **close** (visual + benchmark first) | Project philosophy for visual algorithms; differential case is highest-value |

## Modules

### `src/math/Vec2.h` (extend)
**Responsibility:** add the cross-product building block for signed area / barycentric.
**API:**
- `T cross(Vec2<T> a, Vec2<T> b)` — returns `a.x*b.y - a.y*b.x`, the signed area of the parallelogram (2D cross → scalar).

### `src/rasterizer/primitives/Triangle.h`
**Responsibility:** name the triangle primitive as a plain data aggregate.
**API:**
- `struct Triangle { Vec2i a, b, c; };` — header-only, no methods.

### `src/rasterizer/TriangleRasterizer.h/.cpp`
**Responsibility:** fill a triangle into a `Framebuffer`; two interchangeable strategies.
**API:**
- `void drawTriangleScanline(Triangle t, Color color, Framebuffer& frame_buffer)` — rung 1: sort by y, split at middle vertex, sweep spans.
- `void drawTriangleBbox(Triangle t, Color color, Framebuffer& frame_buffer)` — rung 2: bounding box + per-pixel barycentric inside test.
- (After benchmarking) winner aliased/renamed `drawTriangle`.

### `tests/test_vec2.cpp` (extend)
**Cases:**
- `cross` of known vectors → expected signed scalar; sign flips when arguments swap; collinear → 0.

### `tests/test_triangle_rasterizer.cpp`
**Cases:**
- A filled triangle sets the expected interior pixels; a point outside is untouched.
- Edge/boundary pixels are drawn (the `≥ 0` convention).
- **Differential (highest-value):** `drawTriangleScanline` and `drawTriangleBbox` produce a bit-identical pixel set across several triangles covering different orientations.
- Degenerate (collinear / zero-area) triangle: define and assert the contract (draws nothing vs. a line — decide at implementation).

## Benchmark
Extend `Application::testDrawLineAlgorithms()`-style timing (or a sibling method) to run both triangle rungs over a large randomized set in Release, reading ratios within a run (raw ms drift with thermals), per the Lesson 1 method. Carry forward the finding that this hardware is partly memory-bound and that per-pixel conversions / access patterns dominate over integer-vs-float arithmetic.
