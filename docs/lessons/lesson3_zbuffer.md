# Lesson 3 — Hidden Faces Removal (Z-Buffer)

**Source:** https://haqr.eu/tinyrenderer/z-buffer/

## Goal
Render the OBJ head as a solid, per-pixel-occluded surface — near triangles hide far ones so no back-side geometry bleeds through the front.

## Exit condition
The window shows the head model filled with triangles (flat / per-triangle color for now), depth-correct: the nose sits in front of the cheeks, no far-side face punches through. Plus a depth-test unit case and whatever property/differential tests the depth-aware fill needs. **Status: IN PROGRESS** — steps 1–3 complete (namespace, `Vec3<T>`, OBJ loader, projection, y-flip); the wireframe checkpoint passed and the mesh renders solid with the depth-order artifacts this lesson removes. Step 4 (triangle layer) is designed but unwritten.

## Concepts

### 1. Why per-pixel depth beats sorting (painter's algorithm fails)
You *could* sort all triangles back-to-front and paint far ones first (painter's algorithm). It fails because re-sorting per camera move is expensive **and**, more fundamentally, some configurations have **no valid global order at all** — the killer is **cyclic overlap**: triangle A partly in front of B, B in front of C, C in front of A (A→B→C→A). No sort resolves that. Real meshes produce it in cavities (ear canals, nostrils, the mouth interior). The z-buffer never orders *triangles*; it decides occlusion at the atomic unit that actually matters — the **pixel** — so a single triangle can win some of its pixels and lose others.

### 2. Fragment depth via barycentric interpolation
Each vertex carries its own depth (`a.z`, `b.z`, `c.z`). A pixel inside the triangle gets `z = α·a.z + β·b.z + γ·c.z`. The weights are Lesson 2's three edge-function cross products — the **magnitudes** we threw away, keeping only the sign:
```
α = w0 / (w0 + w1 + w2),  β = w1 / (…),  γ = w2 / (…)
```
Dividing by the total (which *is* the whole triangle's signed area) forces `α+β+γ = 1` automatically — no separate normalization. Exact at the corners: at vertex A the opposite sub-triangle is the whole area → `α=1, β=γ=0` → `z = a.z`; linear in between, which is what depth needs.
- **Consequence — division re-enters the inner loop.** Lesson 2's shipped `drawTriangle` was divisionless (pure sign test). Depth needs the actual weights → a divide, but only for pixels that pass the sign test.
- **Consequence — depth is float.** `a.z…` come from projection as fractional values; the interpolation is fractional. This is why our `Framebuffer` stores `float depth_` rather than the lesson's quantized `unsigned char [0,255]` — we keep full precision.
- **Consequence — the coverage test collapses to one branch.** The *raw* weights' signs follow the winding: CCW → all positive inside, CW → all negative. That is why Lesson 2 needed the winding-agnostic `(all positive) || (all negative)` — two branches, six comparisons. The denominator carries that same winding sign, so **dividing cancels it** and both windings normalise onto the same convention:

  | Winding | raw weights | signed area | α, β, γ |
  |---|---|---|---|
  | CCW | + + + | + | + + + |
  | CW | − − − | − | + + + |

  Inside is therefore always `α >= 0 && β >= 0 && γ >= 0`. The second case did not get handled — normalisation moved both cases onto one sign convention. And the division is not a cost paid for this: α/β/γ are needed for the depth interpolation regardless.
- **Consequence — each weight belongs to the vertex *opposite* its edge.** `cross(c-b, point-b)` → α → pairs with `a.z`; `cross(a-c, point-c)` → β → `b.z`; `cross(b-a, point-a)` → γ → `c.z`. Permuting that mapping still sums to 1 and still covers the right pixels, but tilts the depth gradient wrongly across the face — it looks *almost* right, which is the worst failure mode. Verified numerically (Session 27): those weights reconstruct the sample point exactly via `α·a + β·b + γ·c`.

### 3. The depth test — convention FINALIZED
Two halves that must agree, plus the invariant that the clear value means "nothing here yet / infinitely far" so the first real fragment at any pixel always wins.

| | Choice |
|---|---|
| Clear value | **`0.0f` = far** (the lesson's "black") |
| Nearer means | **larger z** (the lesson's "white") |
| Depth test | keep the fragment when **`z > getDepth(x,y)`**, then `setDepth(x,y,z)` + `setPixel(x,y,color)` |

**String attached:** projected z must stay **≥ 0** (map world z `[-1,1] → [0, C]`) so a freshly-cleared `0.0f` is a genuine far-floor; a fragment at negative z would lose to the clear value and punch a hole. `Framebuffer::getDepth`'s out-of-bounds return of `0.0f` ("farthest") is therefore correct-by-construction. **This kills the `0.0f` placeholder open since Phase 0.2 — it was correct all along.**

### 4. Orthographic projection (+ the `Vec3` and OBJ loader it drags in)
Simplest camera: no perspective, just scale + shift each coordinate. Per vertex `v` in `[-1,1]³`:
```
screen_x = (v.x + 1) · width  / 2      // → [0, width]
screen_y = (v.y + 1) · height / 2      // → [0, height]
depth_z  = (v.z + 1) · C / 2           // → [0, C], stays ≥ 0  (concept 3)
```
Same `(v+1)/2` remap three times — the fullscreen-triangle UV trick again. The exact depth scale `C` is irrelevant to the *test* (only ordering matters); it matters only if we later visualize the depth buffer.
- **`Vec3` becomes necessary** — a vertex is 3D; `Vec2` can't hold it.
- **An OBJ loader becomes necessary** — to get real geometry.
- **Square window:** the head is normalized to a *square* `[-1,1]²`; mapping x by `width/2` and y by `height/2` with `width ≠ height` squashes it (600/800 = 0.75 on y). Fix: **go 800×800** so both axes share one scale. Cheap — it's the `WIDTH`/`HEIGHT` constants; the display texture is sized *from* the `Framebuffer`, so the new size propagates to the GPU with no pipeline surgery. (General letterbox alternative — uniform `min(w,h)/2` + center — pocketed for resizable windows.)

### 5. Back-face culling (the real version)
The head is a closed mesh with **consistent winding**, so a triangle facing away from the camera has the **opposite-sign** signed area. Replace Lesson 2's `abs()` with a **sign check**: keep one orientation, skip the other → ~half the triangles vanish before rasterization. Pure speed.
- **Correctness never depends on it.** On a closed solid, a ray through any pixel enters through a front face *before* reaching the back face on the far side, so the back fragment always loses the depth test anyway. Culling just declines to rasterize what the z-buffer would discard.
- **Precondition is closed AND consistently wound** (a coherently oriented 2-manifold) — not just closed. A single **flipped** face on a closed mesh reads as back-facing on the visible surface → culling deletes it → a **hole** into the interior. So culling *trusts the asset's winding*; the z-buffer trusts nothing and stays the correctness authority.
- **Diagnosis if it goes wrong:** whole model gone / inside-out → wrong global sign (flip the comparison); a localized hole or two → a flipped face in the mesh data, not our code.
- **Naive-to-optimised order:** z-buffer first (correct image), culling second (same image, faster).

### 6. Barycentric coordinates are a 3D concept; the rasterizer needs the xy special case
*(Session 27 — this is what decided where the operations live.)*

True barycentric coordinates **parametrize the surface in 3D**: for a point on the triangle, `P = α·a + β·b + γ·c` in full `Vec3f`, and the coordinates hand back the whole xyz position. That concept is projection-free.

What the rasterizer computes is *not* that. It computes the coordinates with respect to the **xy-projected** triangle, from an integer pixel `(x, y)`. The xy plane is privileged for exactly one reason — it is where the screen is. Nothing geometric prefers it.

**The rasterizer cannot use the 3D version, and the reason is non-negotiable.** The inner loop holds only an integer `(x, y)`. Building a 3D input point requires z, and z is what the weights are being computed to produce. Circular. The cheap xy version exists precisely to *break* that circularity — which is what makes it a rasterization primitive rather than a geometry one.

**Why the xy weights are nonetheless exact, not an approximation.** An **affine map preserves barycentric coordinates**. `orthographicProjection` is affine — multiply-and-add only, no division by a coordinate (Session 25). So a point's coordinates with respect to the projected 2D triangle are *identical* to its coordinates with respect to the 3D triangle. Same numbers.

**The weights are a 2D addressing question; z is merely the first attribute fed to them.** "Which convex combination of the screen vertices is this pixel?" has no z in it. `z = α·a.z + β·b.z + γ·c.z` then applies those weights to per-vertex data. The proof they are separate concerns: the *same* weights later interpolate UVs, colours and normals — impossible if they depended on z.

**Why three z values suffice:** the triangle is planar, so z is a linear function of `(x, y)` and three non-collinear points determine it exactly. At a corner the weights collapse to a single `1` and return that vertex's own z unchanged; in between they sweep smoothly. That gradient is what a per-triangle sort cannot express, and it is why two interpenetrating triangles can each own part of a shared region.

> **Flag for Lesson 5 — perspective-correct interpolation.** Perspective projection is **not** affine (it divides by z), so barycentrics are *not* preserved and screen-linear interpolation of z becomes **wrong**: under perspective `1/z` interpolates linearly, not `z`. Real pipelines interpolate `1/w` and `attribute/w` and divide at the end. Everything in this lesson is correct *because* the projection is orthographic. When the camera lesson swaps in a perspective matrix, the attribute interpolation needs revisiting — the coverage test does not.

**Consequence for architecture:** these operations assume already-projected data, so they do not belong in a space-agnostic geometry namespace. They live in `screen::`. A geometry-side `barycentric` returning a full `Vec3f` position is a legitimate future function — it simply has no caller today and is not written.

## Design decisions
| Decision | Choice | Reason |
|----------|--------|--------|
| Screen-space vertex | **`Vec3f`** (not `Vec2i` + separate z) | `Vec3` is needed anyway for world vertices; the integer-edge optimization is already dead (barycentric forces float division regardless). Float geometry, integer pixel grid. |
| **Triangle types** *(Session 27 — supersedes "Option A")* | **two structs split by coordinate space**: `RasterTriangle { Vec3f a, b, c; }` (screen space, data-only) and `Triangle2D { Vec2i a, b, c; }` (frozen Lesson-2 artifact, keeps its methods) | The name encodes the invariant that the coordinates are **projected**, so the model-space/screen-space ambiguity cannot recur — the same win a `RasterizePrimitive` class buys, for the price of a rename. Keeping `Triangle2D` untouched means `drawTriangleScanline` needs a signature edit only. |
| **Rejected: `Triangle<T>` template** | one concrete `Vec3f` type | `Vec3i` was already disproved in Session 26 — projected z ∈ `[0,1]` truncates to 0 or 1, i.e. two depth levels for the whole model — so the second instantiation cannot exist. It would also force `drawTriangle` to become a template (killing the `.h`/`.cpp` split) or take `Triangle<float>` concretely (buying nothing). |
| **Operations** | **free functions, not methods** | Matches the math layer, where `dot`/`cross`/`normalize` are free functions and there is no `Vec3Utils.h`. Methods on `Triangle` were the outlier. |
| **`screen::` namespace** | holds `twiceSignedArea`, `boundingBox`, `barycentric` — all assume **already-projected** coordinates (x/y are pixels, z is depth) | Rejected `polygon::`: the operations are not space-agnostic geometry (concept 6), and it ended with zero members. `raster::` overclaims — the whole subsystem is rasterization. `projected::` reads badly as an adjective. |
| `sortedByY()` | **stays a `Triangle2D` method**; no `RasterTriangle` equivalent | Its only caller is `drawTriangleScanline`, which stays on `Triangle2D`. A `RasterTriangle` version would have no caller. |
| Lesson-2 2D path | **`drawTriangleScanline` keeps `Triangle2D`** — signature edit only; its tests keep their `Vec2i` literals | Avoids ~8 mechanical cast sites, and its `cross(...) == 0` guard still returns `int`. Cost: `testDrawTriangleAlgorithms()` must build both types from the same source data to compare the two algorithms. |
| **`twiceSignedArea` naming** | not `area`, not `signedArea`, not `doubleSignedArea` | `area` conventionally means the *absolute* value, so a function returning a negative surprises people — "signed" carries real information. `double` reads as a precision claim on a function returning `float`. The 2× is the property no name captures for free, so it must be in the doc comment. |
| `boundingBox` return | `std::pair<Vec2f, Vec2f>` (min corner, max corner), **unrounded and unclamped** | Pixel-grid semantics are the rasterizer's business. `drawTriangle` floors the min, ceils the max, and clamps to `getWidth()`/`getHeight()` in one step — clamping needs framebuffer dimensions, which a geometry function should not know. A 3D bbox was considered and dropped: `.z` has no caller. |
| `barycentric` signature | takes `twice_signed_area` as a **third parameter** | The area is a per-triangle constant; computing it inside would recompute it per pixel and force a divide-by-zero guard into the hot loop. `drawTriangle` already computes and tests it before the loop starts, so threading it in costs one parameter and gives the function a clean precondition. |
| Placement of the three | **declared in `TriangleRasterizer.h`, defined in `TriangleRasterizer.cpp`** | Same translation unit as their only caller, so `barycentric` can inline into the pixel loop without LTO; header declarations keep them reachable from `test_triangle_rasterizer`. Header-only + `inline` was the earlier plan — it solved a CMake problem (adding a source file to two target lists) that does not exist here, since `TriangleRasterizer.cpp` is already in both. |
| `Vec3<T>` API | member `+`, `-`, `*scalar`; **`cross`/`dot` as free functions**; Hadamard `*` **deferred** to shading | Consistent with `Vec2` (free `cross`); no redundant member/free duplication; component-wise multiply has no consumer until Lesson 8–9. Left-scalar `scalar*vec` deferred until needed. |
| `Vec2` retrofit | add ops to `Vec2` opportunistically to match `Vec3` as lessons touch them | Keep the two types from drifting into different mental models. |
| **Namespace** | **`tinymath`** (lowercase); adopt in **one dedicated move-commit BEFORE Lesson 3 code** | The deferred "math namespace" move. Blast radius: `Vec2.h`, `test_vec2.cpp`, `LineDrawer.{h,cpp}`, `TriangleRasterizer.{h,cpp}`, `Triangle.h`. `math` isn't actually taken by std — `tinymath` chosen for clarity. |
| OBJ loader | `src/io/ObjLoader.*`, free `loadObj(path)` → minimal mesh (`vector<Vec3f>` verts + `vector<array<int,3>>` faces); parse only `v` and the position index of each `f` group, ignore `vt`/`vn` | Asset I/O, not math or rasterization. Positions are all this lesson needs; UV/normal storage arrives with textures/shading. |
| Projection | **free function in `tinymath`**, orthographic scale/shift now; **`Matrix4x4`-backed in Lesson 5** | Projection is coordinate-space math (its home is `tinymath` from day one); but a matrix to express a scale-and-shift is premature — build the matrix when the camera lesson demands it. |
| Projection naming | **`orthographicProjection(point_position, …)`** — not bare `project(v, …)` | Explicit name lets a future `perspectiveProjection` slot in beside it with no rename (naming for clarity ≠ stubbing an unused function, which was rejected). `point_position` carries no mesh assumption; the function is pure coordinate-space math. |
| **Y-axis flip placement** | **inside `Framebuffer::index()`** — `(height_ - 1 - y) * width_ + x` — *not* inside the projection | Model y grows up, screen rows grow down; one of the two must reverse it. `index()` is a single choke point shared by `setPixel`/`getPixel`/`setDepth`/`getDepth`, so colour and depth flip together by construction and the whole rasterizer thinks y-up. `getData()` stays raw top-left for the Vulkan upload, so the display pipeline was untouched. Cost: Lesson 1–2 positional tests had to be re-anchored (see Modules). |
| Depth test placement | inside `drawTriangle`'s inner loop: `z > getDepth(px,py)` → `setDepth` + `setPixel` | The rasterizer owns the read-modify-write of the `Framebuffer`'s depth buffer. |
| Pixel sample point | corner (`px,py`) vs center (`px+0.5,py+0.5`) — **decide when writing `drawTriangle`** | Center is more correct for coverage/tie-breaking; corner is simpler and matches Lesson 2. |
| `cross` spelling on `Vec3` verts | **RESOLVED** — build a `Vec2f` from `.x/.y` and reuse the existing templated `cross(Vec2,Vec2)`, which returns a scalar. Hidden inside `screen::twiceSignedArea` and `screen::barycentric` | No new math function needed. `cross(Vec3f,Vec3f)` returns a `Vec3f`, so `== 0` would not even compile. |

## Implementation order (dependency-ordered)
1. **`tinymath` namespace move-commit** — wrap `Vec2` + all users. Cleanup on existing code first, its own commit.
2. **`Vec3<T>`** — storage + member `+`/`-`/`*scalar`, free `cross`/`dot`. `tests/test_vec3.cpp`.
3. **Projection** (free fn in `tinymath`) + **`io/ObjLoader`** — both produce/consume `Vec3f`.
4. **Triangle layer** *(revised Session 27)* — `Triangle.h`: rename `Triangle` → `RasterTriangle`, keep `Triangle2D`, no namespace and no functions in the file. `TriangleRasterizer.h`: add `namespace screen` with the three declarations + contract doc comments; `drawTriangleScanline` takes `Triangle2D`, `drawTriangle` takes `RasterTriangle`. `TriangleRasterizer.cpp`: write the three bodies.
5. **`drawTriangle` rewrite** — `twiceSignedArea` once above the loop (zero → early return), bbox floored/ceiled/clamped to the framebuffer, then per pixel: `barycentric` → `α,β,γ >= 0` → `z = α·a.z+β·b.z+γ·c.z` → `z > getDepth` → `setDepth` + `setPixel`.
6. **Square window (800×800)** + wire the head into the producer; visual check against the "before" picture; tests. Add real back-face culling last (same image, faster) once the head renders correctly.

## Modules

### `src/math/Vec3.h` (new)
**Responsibility:** 3D vector — world vertex and (post-projection) screen vertex with depth.
**API:**
- `struct Vec3<T> { T x, y, z; };` templated; `using Vec3f = Vec3<float>;`
- members `operator+`, `operator-`, `operator*(T scalar)`
- free `T dot(Vec3<T>, Vec3<T>)`, free `Vec3<T> cross(Vec3<T>, Vec3<T>)`

### `src/math/Vec2.h` (move into `tinymath`, retrofit)
**Responsibility:** existing 2D vector, now namespaced; op set kept in step with `Vec3` as needed.

### `src/math/Projection.h/.cpp` (new, in `tinymath`)
**Responsibility:** coordinate-space remap from normalized model space into screen space.
**API:**
- `Vec3f orthographicProjection(Vec3f point_position, int screen_width, int screen_height, float depth_scale)` — orthographic `(v+1)·scale/2` remap on all three axes; z kept ≥ 0. Matrix-backed in Lesson 5.

**Naming:** `orthographicProjection` (not bare `project`) so a future `perspectiveProjection` slots in beside it without a rename — naming for clarity, *not* a stubbed second function. Parameter is `point_position`, not `vertex`: the function is coordinate-space math and carries no mesh assumption.

**No y-flip here.** The screen-space y reversal lives in `Framebuffer::index()` (see below), so this stays a pure scale-and-shift on all three axes.

**Assumption:** input is already normalized to `[-1,1]` — a property of `african_head.obj`, not something this function enforces. Feed it an unnormalized model and it silently returns off-screen coordinates.

**`.h`/`.cpp` split** (not header-only `inline`): the function is not templated, runs per-vertex rather than per-pixel (~1258 calls — inlining is noise), and the split removes the `inline` requirement permanently.

### `src/io/ObjLoader.h/.cpp` (new)
**API:**
- `Mesh loadObj(std::string const& path)` — `Mesh { std::vector<Vec3f> vertices; std::vector<std::array<int,3>> faces; }`. Parse `v` lines and the first (position) index of each `f` group; skip `vt`/`vn`.

### `src/rasterizer/primitives/Triangle.h` (change)
**Responsibility:** the triangle value types — **structs only. No methods on `RasterTriangle`, no namespace, no free functions in this file.**
**API:**
- `struct RasterTriangle { tinymath::Vec3f a, b, c; };` — screen space: `.x`/`.y` are pixel coordinates, `.z` is depth in `[0,1]`. Renamed from `Triangle` so the *type* records that the coordinates are already projected.
- `struct Triangle2D { tinymath::Vec2i a, b, c; };` — frozen Lesson-2 artifact, deprecated, retains `sortedByY()` / `getXBounds()` / `getYBounds()`. Reached only by `drawTriangleScanline`.

**No geometry-side triangle type exists, and none is needed.** `io::Mesh` (`vector<Vec3f> vertices` + `vector<array<int,3>> faceIndices`) **is** the model-space representation, and it is the *indexed* one — a standalone geometry triangle would denormalize it and copy every vertex ~6× on a closed mesh. Lesson 5's face normal pulls three `Vec3f` out of the mesh by index, crosses two edges and normalizes: a free function over three vertices, no struct.

### `src/rasterizer/TriangleRasterizer.h/.cpp` (change)
`namespace screen` is **declared in this header and defined in this `.cpp`** — see the placement row in Design decisions. The namespace/filename mismatch is deliberate and temporary; it moves into `RasterizePrimitive`'s file when that is extracted.

**`namespace screen` API** — all three assume already-projected coordinates:

- `float twiceSignedArea(const RasterTriangle& triangle)` — `cross(b-a, c-a)` on the **xy projection only, z ignored**. Three facts the doc comment must state, because none is visible from the signature:
  1. **xy-only.** With `Vec3f` members and (formerly) a 3D neighbour, a reader will otherwise assume the 3D parallelogram area — a different number, and wrong as the barycentric denominator.
  2. **Signed.** The sign is the winding, which is what back-face culling reads.
  3. **Twice** the geometric area — i.e. the *parallelogram* area, since the triangle is half of it. The factor is load-bearing: the edge weights carry the same factor, which is the only reason `α+β+γ = 1`. "Fixing" it with a `/2` silently doubles all three weights.

  Verified numerically (Session 27): the choice of shared vertex is irrelevant — `cross(b-a,c-a)`, `cross(c-b,a-b)`, `cross(a-c,b-c)` all agree. **Operand order flips the sign.** And `w0+w1+w2` equals `cross(b-a,c-a)` exactly, confirming it already matches the existing weight convention.

  *Why not `|cross₃|`:* a magnitude has no sign, so it can never drive culling; and an edge-on triangle has collinear screen x/y but differing `.z`, so `|cross₃|` is non-zero while screen coverage is zero — the degeneracy guard would pass and step 5 would divide by zero.

- `std::pair<tinymath::Vec2f, tinymath::Vec2f> boundingBox(const RasterTriangle& triangle)` — min corner, max corner. **Unrounded, unclamped**; the caller owns pixel-grid semantics.

- `tinymath::Vec3f barycentric(const RasterTriangle& triangle, tinymath::Vec2f point, float twice_signed_area)` — returns (α, β, γ) packed so **`.x` = α = vertex `a`'s weight**. Doc comment must state the packing *and* the precondition: `twice_signed_area` must be non-zero and must come from *this* triangle. Nothing in the type system enforces either; a stale area yields weights that are silently wrong rather than obviously broken.

**`drawTriangle(const RasterTriangle&, Color, Framebuffer&)`** — `twiceSignedArea` once (zero → return), bbox floored/ceiled/**clamped to `getWidth()`/`getHeight()`**, then per pixel: `barycentric` → single-branch `α,β,γ >= 0` → `z = α·a.z+β·b.z+γ·c.z` → `z > getDepth` → `setDepth` + `setPixel`. The clamp is a real optimisation and a benchmarkable rung: today the loop walks the full bbox and lets `setPixel` silently reject out-of-range writes, so a mostly-off-screen triangle burns an entire inner loop doing nothing.

**Float truncation trap:** `static_cast<int>` rounds **toward zero** — `floor` for positive coordinates but `ceil` for negative ones. Faces projecting off-screen left or below therefore need explicit `floor`/`ceil`, not a bare cast.

**`drawTriangleScanline(Triangle2D, Color, Framebuffer&)`** — unchanged apart from the parameter type. No depth support, no future role; kept as the Lesson-2 benchmark rung.

### `RasterizePrimitive` — designed, deliberately deferred
Proposed in Session 27: an immutable class holding projected vertices plus the derived setup values, owning these operations. **This is what real hardware does** — GPUs have a *triangle setup* stage that emits exactly this (edge coefficients, reciprocal area, screen bbox) for the fragment stage to consume, and it is the natural shape for the Phase 2 compute port.

**Deferred because the member list is not yet known.** Today it is area + bbox. Lesson 5 adds `1/w` once the projection stops being affine; textures add UV references; Gouraud adds per-vertex normals or colours. Extracting the class *after* `drawTriangle` works reads that list off working code; designing it now guesses it.

**Constraint for when it is built: make it immutable** — const members, computed once in the constructor, no setters. Cached derived state plus mutable vertices is a staleness bug (change a vertex, the stored area is silently stale) that the current explicit-precondition design cannot have.

### `src/rasterizer/Framebuffer.h/.cpp` (change) — y-axis origin flip
**Decision:** the framebuffer origin moves to **bottom-left**. `index()` becomes `(height_ - 1 - y) * width_ + x`.

**Why here and not in `orthographicProjection`:** the model's y grows up, screen rows grow down, so *something* must reverse y. Both placements work. Chosen: `Framebuffer`, because `index()` is a single private choke point that `setPixel`, `setDepth`, `getPixel` and `getDepth` all route through — colour and depth therefore flip together by construction, and the whole rasterizer gets to think y-up. `getData()` deliberately stays **raw top-left** row order, which is exactly what the Vulkan upload wants, so the display pipeline needed no change.

**Consequence — the two coordinate systems must be kept straight:**
| Path | Origin |
|---|---|
| `setPixel` / `getPixel` / `setDepth` / `getDepth` | bottom-left (y-up) |
| `getData()` | top-left (raw memory) |

**New API:** `Color getPixel(int x, int y) const` — added because tests had no accessor to read colour back and were hand-rolling `getData() + (y*width + x)`, bypassing `index()` entirely. Returns `Color{0,0,0,0}` out of bounds (same ambiguity as `getDepth`'s `0.0f`: indistinguishable from a legitimate black transparent pixel — fine for tests, a trap if rasterizer code ever branches on it).

**`Color` gains `bool operator==(Color const&) const = default;`** (C++20 memberwise) so tests can compare colours directly.

**Two testing lessons banked (both cost real debugging time):**
1. *A test must read through the same abstraction it wrote through.* The six drawing-test failures were one bug in two `isSet()` helpers, not six mirrored expectations.
2. *A symmetric test case can be geometrically incapable of failing.* `drawLine vertical` (y = 2..13 in a 16-tall buffer) passed through the flip because that range maps onto itself; `drawLine horizontal` (y = 8) failed. Same class of blind spot as the `Vec3::cross` typo that survived `{1,0,0}×{0,1,0}`. Anchor tests against raw memory, and pick asymmetric coordinates.

### `tests/test_vec3.cpp` (new)
**Cases:** `+`/`-`/`*scalar`; `dot` known values; `cross` known values, anti-commutativity, parallel → zero.

### `tests/test_framebuffer.cpp` (extend)
**Cases:** depth clear = `0.0f`; `setDepth`/`getDepth` round-trip; the `z > getDepth` keep/discard decision.
**Added for the flip (done):** `getPixel` agrees with raw memory layout (the *anchor* — the only case that can catch an off-by-one inside `index()`, since every other test cancels the error across write and read); `getPixel` round-trips `setPixel` at asymmetric rows with distinct colours; `getPixel` out of bounds returns zero (buffer pre-cleared to non-zero so a stray in-bounds read can't masquerade as the OOB result); `getPixel` reads the clear colour at every pixel (`clear()` bypasses `index()`).

### `tests/test_triangle_rasterizer.cpp` (change)
**Cases — `drawTriangle`:** depth-aware fill writes both color and depth for interior pixels; a nearer triangle overwrites a farther one at a shared pixel; a farther one does not; degenerate (zero-area) draws nothing.

**Cases — `screen::` (reachable because they are declared in the header):**
- `twiceSignedArea` — known value on an asymmetric triangle; **sign flips when two vertices are swapped** (the culling contract); zero for collinear vertices; unchanged when `.z` varies (proves the xy-only claim).
- `barycentric` — at each vertex returns a single `1` and two `0`s (the corner check that catches a permuted α/β/γ mapping); at the centroid returns ⅓,⅓,⅓; weights sum to 1; **all three non-negative for an interior point under *both* windings** — the case that proves the single-branch coverage test.
- `boundingBox` — corners for a triangle with negative coordinates, since that is where `static_cast<int>`'s round-toward-zero differs from `floor`.

> Per the Session-24/25 test-design lessons: pick **asymmetric** coordinates with every term non-zero. A symmetric case can be geometrically incapable of failing — it is what let the `Vec3::cross` typo and the y-flip bug both survive their first test.

## Open questions / carry-forward
- ~~**Scanline rung fate**~~ — **resolved**: kept, on `Triangle2D`, signature edit only.
- ~~**`cross` spelling** on `Vec3` verts~~ — **resolved**: `Vec2f` round-trip inside `screen::` (see Design decisions).
- **Pixel sample point** (corner vs center) — still open; decide when writing `drawTriangle`.
- **`Triangle2D`'s fate.** Kept "for posterity" with a deprecation comment, reachable only by `drawTriangleScanline`. Unresolved: git history is already the museum, and a deprecated struct in a live header tends to read as an option rather than a fossil.
- **`screen::` + `RasterTriangle` is mildly redundant** — either qualifier alone would carry the space. Explicitly agreed not to reopen; keep both or drop one.
- **`Vec2` still has only `operator-`** (no `+`, no `*`) — the "retrofit opportunistically" decision never happened. The `Vec3f`→`Vec2f` `.z`-dropping extraction is written longhand by component ~5 times across `twiceSignedArea` and `barycentric`; a `tinymath::xy(Vec3<T>) -> Vec2<T>` helper was suggested and left to judgement after writing the repetition once. A transposed `.x`/`.y` is the thing that hides there.
- **Near-degenerate triangles:** `twice_signed_area` is float where Lesson 2's was exact `int`. A nearly-degenerate triangle gives a tiny denominator and enormous weights. Note it; no epsilon until it misbehaves.
- **No Release build config exists** — `CMakeLists.txt` sets no `CMAKE_BUILD_TYPE`, no `-O` flag and no LTO, so nothing inlines today in any arrangement, and every `Timer` benchmark so far has measured unoptimized code. Relative comparisons between rungs remain valid; absolute numbers do not. Worth fixing before the next benchmark is taken seriously.
- **`Matrix4x4`** — introduced in Lesson 5 (camera); projection rebuilt on it then. Do **not** build it this lesson.
- **Deferred `Application` cleanups** (Lesson 1–2): dead `drawPoint()`/`drawTestPattern()`, `std::array<Vec2i,2>` → `Line`, stale bench `cout` labels — fold in opportunistically, non-blocking.
- ~~**Window** flips 800×600 → 800×800 as part of Lesson 3 setup.~~ **Done.**
- **Right-edge off-by-one in `orthographicProjection`** — `point_position.x == +1` maps to exactly `screen_width`, but the last valid column is `screen_width - 1`, so an extreme-edge vertex is silently dropped by `setPixel`'s bounds check. Same on y. Undecided: accept it (TinyRenderer does) vs. scale by `width - 1` vs. clamp. Revisit if the wireframe shows a missing edge.
- **Assets are gitignored** — `models/obj/` is excluded (`*.obj`, `*.tga`, `*.png`, bare `obj`). A fresh clone has no model and `loadObj` will throw. `README.md` needs a line on where to obtain the assets before this is committed.
