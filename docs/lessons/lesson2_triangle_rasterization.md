# Lesson 2 — Triangle Rasterization

**Source:** https://haqr.eu/tinyrenderer/rasterization/

## Goal
Fill a triangle with solid color so that flat shapes — and ultimately the OBJ head — read as surfaces instead of wireframes.

## Exit condition
The window shows at least one solid-filled triangle drawn from three hardcoded `Vec2i` points, and `tests/test_triangle_rasterizer.cpp` passes. **DONE (Sessions 21–22).**

> **Amended:** the planned bit-identical *differential* case was **dropped**. Scanline (float-`t` edge interpolation) and barycentric (exact integer edge functions) legitimately disagree on edge pixels, and we ship only one rung — so coupling them would test a coincidence. The test verifies each rung independently: interior fills, exterior stays clear, boundary drawn (shipped rung), degenerate draws nothing.

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
| Winner naming | Named the rung by strategy (`drawTriangleScanline` / `drawTriangleBarycentric`); winner renamed `drawTriangle` after benchmarking | Same close-out as Lesson 1; don't pick a winner before measuring |
| **Rung 2 name** | `drawTriangleBbox` → **`drawTriangleBarycentric`** (then winner → `drawTriangle`) | The bbox is only a pixel-enumeration optimization; the defining idea (and the axis of contrast with scanline) is the per-pixel **barycentric** inside test |
| **Benchmark winner** | Scanline is faster on scalar CPU (~2.4–2.9×); **barycentric is named `drawTriangle`** anyway | Barycentric is the mesh/GPU-portable path (Phase 2); the CPU loss is a serial-execution artifact — see findings below |
| Triangle representation | `struct Triangle { Vec2i a, b, c; }` — data object, no methods | A domain object that holds points; performs no arithmetic on itself, so it is not a math type |
| `Triangle` location | `src/rasterizer/primitives/Triangle.h` | It is a rasterizer primitive, not linear algebra; transforms-as-components may live alongside later |
| `Vec2` location | Stays in `src/math/` | Pure math operand — gets `cross` + `operator-` this lesson; it *is* algebra |
| Signed-area source | Free `cross(Vec2, Vec2)` → scalar in `Vec2.h`; `operator-` added as a **member** | 2D cross is a scalar = signed area; subtraction is inherently ordered so member is fine. (`cross` free deviates toward the CLAUDE.md convention; both kept in global namespace — see open questions) |
| Winding policy | Inside test is `all wi >= 0` **OR** `all wi <= 0` (divisionless equivalent of `abs`); cull hook comment-marked | One hardcoded triangle must draw regardless of winding; real single-sign cull deferred to Lesson 3 |
| **Inside test** | Sign test on the three edge-function cross products — **no division** | A solid fill needs only "is P inside," not the α/β/γ values; the divide (actual weights) returns in Lesson 3 for UV/normal/z interpolation |
| **Degenerate contract** | Zero-area (`cross(b-a, c-a) == 0`) → **draws nothing**, guarded in *both* rungs | Same category as the zero-length line; also protects scanline from a `0/0` on fully-collinear input |
| Tests | Written at lesson **close** (visual + benchmark first); property-based per rung, no differential | Project philosophy for visual algorithms; see the exit-condition amendment |

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
- `void drawTriangleScanline(Triangle t, Color color, Framebuffer& frame_buffer)` — rung 1: sort by y, split at middle vertex, sweep spans. **Kept** as the benchmark reference rung. Flat-top handled by the strict `<` upper loop; flat-bottom `0/0` guarded (Session 22); collinear guarded by the shared degenerate check.
- `void drawTriangle(Triangle t, Color color, Framebuffer& frame_buffer)` — rung 2 (was `drawTriangleBarycentric`): bounding box + per-pixel barycentric sign test. **The shipped winner.**
- `Triangle` gained `getXBounds()` / `getYBounds()` (`std::pair<int,int>`, min/max over the three verts) for the bbox.

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
`Application::testDrawTriangleAlgorithms()` times both rungs over a randomized triangle set in Release, reading ratios within a run (raw ms drift with thermals), per the Lesson 1 method.

### The measured result (Session 22) — the ladder is inverted again
| Triangle size | scanline | barycentric | ratio (bary / scan) |
|---------------|----------|-------------|----------------------|
| Huge (full-screen random) | 89.6 ms | 255.7 ms | **2.85×** |
| Small (≤ 100×100) | — | — | ~similar |
| Tiny (20×20) | 4.77 ms | 11.56 ms | **2.42×** |

**Findings:**
1. **Barycentric is ~2.4–2.9× slower on scalar CPU, and the ratio is roughly size-independent.** A triangle covers **at most half its bounding box** (max = the axis-aligned right triangle), so barycentric tests ≤ 2× the covered pixels — a *size-independent* ceiling. The measured >2× is that bbox waste **times** a heavier per-pixel test (3 subtractions + 3 cross products on every bbox pixel, including the wasted outside-half, which is pure ALU) versus scanline's cheap per-span `setPixel`.
2. **Small triangles barely close the gap** (2.85× → 2.42×). The only lever is amortizing scanline's per-triangle / per-row fixed overhead (sort, two float divisions per row) over fewer pixels — a second-order effect that fights the 2× bbox floor.
3. **Barycentric is still the winner going forward.** Its real advantage — every pixel is *independent* (no loop-carried state, unlike scanline's marching edges) — is **invisible in a single-threaded scalar loop**. It only cashes out as SIMD lanes / GPU threads doing those independent tests in parallel: 2× more work across thousands of lanes beats serial edge-walking. That is *why* real GPUs use edge-function/barycentric rasterization, and it is the Phase 2 payoff.

Same shape as Lesson 1: the "naive/old" rung benchmarks faster on this hardware, but the rung built for where we're going (the GPU) is the one we keep.

## Open questions / carry-forward
- **`math::` namespace:** `cross` is a free function in the *global* namespace (matching global `Vec2`). Deferred a real `math::` namespace until the layer grows (Vec3, Matrix4x4, dot, normalize) — do it in one wholesale move then, not piecemeal.
- **Deferred cleanups** (from Lesson 1, still open): dead `drawPoint()` / `drawTestPattern()` in `Application`, the `std::array<Vec2i,2>` → `Line` struct question, and the `std::cout` benchmark labels still reading "rasterize"/"barycentric".
- **Loop-invariant hoist:** the three edge vectors in `drawTriangle` are recomputed per pixel; left un-hoisted (compiler likely handles it at `/O2`). Revisit if a future profile says otherwise.
- Back-face culling (single-sign test replacing the `>=0 OR <=0`) wired for real in Lesson 3 with the OBJ head's consistent winding.
