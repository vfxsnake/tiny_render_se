# Lesson 3 — Hidden Faces Removal (Z-Buffer)

**Source:** https://haqr.eu/tinyrenderer/z-buffer/

## Goal
Render the OBJ head as a solid, per-pixel-occluded surface — near triangles hide far ones so no back-side geometry bleeds through the front.

## Exit condition
The window shows the model filled with triangles (flat / per-triangle color for now), depth-correct: the nose sits in front of the cheeks, no far-side face punches through. Plus a depth-test unit case and whatever property/differential tests the depth-aware fill needs.

**Status: DONE.** `diablo3_pose.obj` renders solid and depth-correct in the live window — front and back surfaces resolve per pixel and the file-order overlap artifacts of the "before" picture are gone. Back-face culling ships as an optional flag on `drawTriangle` and measures **2.02× faster** on 5022 triangles (11.71 ms → 5.79 ms, Release, 2314 of 5022 culled). `tests/test_triangle_rasterizer.cpp` covers the three `screen::` functions, depth interpolation, order-independence of the depth test, and culling.

## Concepts

### 1. Why per-pixel depth beats sorting (painter's algorithm fails)
You *could* sort all triangles back-to-front and paint far ones first (painter's algorithm). It fails because re-sorting per camera move is expensive **and**, more fundamentally, some configurations have **no valid global order at all** — the killer is **cyclic overlap**: triangle A partly in front of B, B in front of C, C in front of A (A→B→C→A). No sort resolves that. Real meshes produce it in cavities (ear canals, nostrils, the mouth interior). The z-buffer never orders *triangles*; it decides occlusion at the atomic unit that actually matters — the **pixel** — so a single triangle can win some of its pixels and lose others.

### 2. Fragment depth via barycentric interpolation
Each vertex carries its own depth (`a.depth`, `b.depth`, `c.depth`). A pixel inside the triangle gets `z = α·a.depth + β·b.depth + γ·c.depth`. The weights are Lesson 2's three edge-function cross products — the **magnitudes** we threw away, keeping only the sign:
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

### 7. After projection, depth is an attribute — not a third coordinate
*(Session 28 — this reversed the planned `RasterTriangle { Vec3f a, b, c; }` design.)*

A projected vertex looks like three floats, but they are not three of the same thing. `x` and `y` are positions on the pixel grid, in pixel units, and they answer *where*. `z` is a number in `[0, 1]` that answers *how near* — it shares no units with x/y and no geometric operation treats it as a spatial axis. Packing all three into a `Vec3f` and calling the last one `.z` invites the operation the whole layer must never do: crossing two projected vertices and calling the result a normal. (That is exactly the Lesson-5 trap already banked — a face normal must come from **model-space** vertices.)

So the type splits the two: `RasterVertex { Vec2f coordinates; float depth; }`, and `Triangle` holds three of them. "Positions are xy-only" stops being a doc comment and becomes structural — `twiceSignedArea`, `boundingBox` and `barycentricWeights` cannot reach a third coordinate because there isn't one to reach.

**Why not parallel fields** (`Vec2f a, b, c` plus `float a_depth, b_depth, c_depth`), the first proposal: the vertex↔depth pairing would exist only in the *names*, so any code reordering vertices desynchronises them silently. And depth is the **first** per-vertex attribute, not the only one — UVs, normals and `1/w` all follow — so the parallel form grows into five parallel triples. `RasterVertex` carries the pairing structurally and survives the growth.

**Why the type is just `Triangle`, with no qualifier.** A qualifier earns its keep only against a sibling that exists, and no second triangle type ever will: face normals read three `Vec3f` out of `io::Mesh` by index and discard them, culling reads the screen-space signed area, UVs ride *inside* `RasterVertex`, perspective adds an `inverse_w` member, shadow mapping reuses the same type. `io::Mesh` already **is** the model-space representation, and it is the indexed one — a geometry triangle would only denormalize it.

### 8. Screen coordinates stay in floating point
*(Session 28.)*

The tempting argument for `int` screen coordinates: the framebuffer is a grid of discrete cells, so quantizing vertices to it is expected loss, not real loss.

It isn't. The grid quantizes **where you sample**, not **where the edge is**. Snapping a vertex moves the geometry *before* you measure it — by up to half a pixel, in a direction unrelated to the sample grid — which is a different operation from sampling a continuous signal on a fixed lattice. Two concrete costs:

- **Coverage-based anti-aliasing becomes impossible.** "What fraction of this pixel does the triangle cover?" is a sub-pixel question, and rounding the vertices destroys the input before it can be asked. This was the deciding argument.
- **`static_cast<int>` truncates toward zero**, so an int vertex bakes an asymmetric rounding rule into the *geometry* rather than leaving it at the bounding-box stage where it belongs.

The counter-argument for `int` is real and was not dismissed: integer coordinates give an **exact** signed area (no float error near degeneracy) and watertight shared edges. Real hardware takes neither branch — it uses **fixed-point** (e.g. 24.8) to get exactness *and* sub-pixel precision. For this lesson's static-mesh exit condition `int` would have been adequate; the cost of choosing it is deferred, not absent.

## Design decisions
| Decision | Choice | Reason |
|----------|--------|--------|
| **Screen-space vertex** *(Session 28 — supersedes the `Vec3f` row below)* | **`RasterVertex { tinymath::Vec2f coordinates; float depth; }`** | After projection, depth is an interpolated attribute in `[0,1]`, not a spatial axis in pixel units (concept 7). Splitting it out makes "positions are xy-only" structural rather than a doc comment, and blocks the Lesson-5 trap of crossing two *projected* vertices to get a normal. |
| ~~Screen-space vertex~~ *(superseded)* | ~~**`Vec3f`** (not `Vec2i` + separate z)~~ | Was: `Vec3` is needed anyway for world vertices, and the integer-edge optimization is already dead. The float half survives (concept 8); the packing into one `Vec3f` did not. |
| **Rejected: parallel depth fields** | not `Vec2f a, b, c` + `float a_depth, b_depth, c_depth` | The vertex↔depth pairing would live only in the *names*, so anything reordering vertices desyncs them silently. And depth is the first per-vertex attribute, not the only one — UVs, normals, `1/w` follow — so it grows into five parallel triples. |
| **Triangle type** *(Session 28 — supersedes the `RasterTriangle` row below)* | **`Triangle { RasterVertex a, b, c; }`** — no qualifier; `Triangle2D { Vec2i a, b, c; }` remains as the frozen Lesson-2 artifact | A qualifier earns its keep only against a sibling that exists, and no second triangle type ever will (concept 7): `io::Mesh` already *is* the indexed model-space representation, and every later feature either reads it by index or rides inside `RasterVertex`. |
| ~~Triangle types~~ *(superseded)* | ~~`RasterTriangle { Vec3f a, b, c; }` + `Triangle2D`~~ | Was: the name encodes that the coordinates are projected. The ambiguity is now solved by `RasterVertex`'s field names instead, so the prefix bought nothing. |
| **Rejected: `Triangle<T>` template** | one concrete type | `Vec2i` screen coordinates were rejected on their own merits (concept 8), so the second instantiation cannot exist. It would also force `drawTriangle` to become a template (killing the `.h`/`.cpp` split) or take `Triangle<float>` concretely (buying nothing). |
| **Operations** | **free functions, not methods** | Matches the math layer, where `dot`/`cross`/`normalize` are free functions and there is no `Vec3Utils.h`. Methods on `Triangle` were the outlier. |
| **`screen::` namespace** | holds `twiceSignedArea`, `boundingBox`, `barycentric` — all assume **already-projected** coordinates (x/y are pixels, z is depth) | Rejected `polygon::`: the operations are not space-agnostic geometry (concept 6), and it ended with zero members. `raster::` overclaims — the whole subsystem is rasterization. `projected::` reads badly as an adjective. |
| `sortedByY()` | **stays a `Triangle2D` method**; no `RasterTriangle` equivalent | Its only caller is `drawTriangleScanline`, which stays on `Triangle2D`. A `RasterTriangle` version would have no caller. |
| Lesson-2 2D path | **`drawTriangleScanline` keeps `Triangle2D`** — signature edit only; its tests keep their `Vec2i` literals | Avoids ~8 mechanical cast sites, and its `cross(...) == 0` guard still returns `int`. Cost: `testDrawTriangleAlgorithms()` must build both types from the same source data to compare the two algorithms. |
| **`twiceSignedArea` naming** | not `area`, not `signedArea`, not `doubleSignedArea` | `area` conventionally means the *absolute* value, so a function returning a negative surprises people — "signed" carries real information. `double` reads as a precision claim on a function returning `float`. The 2× is the property no name captures for free, so it must be in the doc comment. |
| `boundingBox` return *(Session 28)* | **`struct BBox { float x_min, y_min, x_max, y_max; }`**, unrounded and unclamped — not `std::pair<Vec2f, Vec2f>` | A `pair` does not say which element is the minimum; the struct names it. Pixel-grid semantics stay the rasterizer's business — clamping needs framebuffer dimensions, which a geometry function should not know. A 3D bbox was considered and dropped: depth has no caller. |
| **Weight-triple return type** *(Session 28)* | **`struct BarycentricWeights { float alpha, beta, gamma; }`** — not `Vec3f`, `std::array<float,3>` or `std::tuple` | Weights are not a vector: `dot`/`cross`/`normalize` are meaningless on them, and `Vec3f` advertises all three. `std::array` overcorrects — positional access is exactly where the vertex pairing gets transposed. **Structured bindings work on any aggregate**, so the struct is a strict superset of the tuple: same `auto [alpha, beta, gamma] = …` call site, plus a self-documenting signature, plus named members for when the weights get passed on (interpolating UVs in L4/5 would otherwise force `std::get<0>`). |
| `barycentricWeights` signature | takes `twice_signed_area` as a **third parameter** | The area is a per-triangle constant; computing it inside would recompute it per pixel and force a divide-by-zero guard into the hot loop. `drawTriangle` already computes and tests it before the loop starts, so threading it in costs one parameter and gives the function a clean precondition. |
| **Back-face culling switch** | `bool cull_back_faces = true` — a defaulted **fourth parameter** on `drawTriangle`, defaulted **on the declaration** | Culling is a pure optimization, so the caller must be able to turn it off to prove the image is unchanged, and the A/B benchmark needs both paths in one build. A default argument written in the `.cpp` is invisible to callers, and repeating it in both places is a hard error. |
| Degenerate vs back-facing | merged into `area == 0 → return` followed by `area < 0 → cull` | Two separate guards, but the degenerate case returns unconditionally because a zero area is the barycentric divisor. Fine for a closed mesh; the day a single-sided plane is drawn, the zero case may want to draw rather than skip. |
| Placement of the three | **declared in `TriangleRasterizer.h`, defined in `TriangleRasterizer.cpp`** | Same translation unit as their only caller, so `barycentric` can inline into the pixel loop without LTO; header declarations keep them reachable from `test_triangle_rasterizer`. Header-only + `inline` was the earlier plan — it solved a CMake problem (adding a source file to two target lists) that does not exist here, since `TriangleRasterizer.cpp` is already in both. |
| `Vec3<T>` API | member `+`, `-`, `*scalar`; **`cross`/`dot` as free functions**; Hadamard `*` **deferred** to shading | Consistent with `Vec2` (free `cross`); no redundant member/free duplication; component-wise multiply has no consumer until Lesson 8–9. Left-scalar `scalar*vec` deferred until needed. |
| `Vec2` retrofit | add ops to `Vec2` opportunistically to match `Vec3` as lessons touch them | Keep the two types from drifting into different mental models. |
| **Namespace** | **`tinymath`** (lowercase); adopt in **one dedicated move-commit BEFORE Lesson 3 code** | The deferred "math namespace" move. Blast radius: `Vec2.h`, `test_vec2.cpp`, `LineDrawer.{h,cpp}`, `TriangleRasterizer.{h,cpp}`, `Triangle.h`. `math` isn't actually taken by std — `tinymath` chosen for clarity. |
| OBJ loader | `src/io/ObjLoader.*`, free `loadObj(path)` → minimal mesh (`vector<Vec3f>` verts + `vector<array<int,3>>` faces); parse only `v` and the position index of each `f` group, ignore `vt`/`vn` | Asset I/O, not math or rasterization. Positions are all this lesson needs; UV/normal storage arrives with textures/shading. |
| Projection | **free function in `tinymath`**, orthographic scale/shift now; **`Matrix4x4`-backed in Lesson 5** | Projection is coordinate-space math (its home is `tinymath` from day one); but a matrix to express a scale-and-shift is premature — build the matrix when the camera lesson demands it. |
| Projection naming | **`orthographicProjection(point_position, …)`** — not bare `project(v, …)` | Explicit name lets a future `perspectiveProjection` slot in beside it with no rename (naming for clarity ≠ stubbing an unused function, which was rejected). `point_position` carries no mesh assumption; the function is pure coordinate-space math. |
| **Y-axis flip placement** | **inside `Framebuffer::index()`** — `(height_ - 1 - y) * width_ + x` — *not* inside the projection | Model y grows up, screen rows grow down; one of the two must reverse it. `index()` is a single choke point shared by `setPixel`/`getPixel`/`setDepth`/`getDepth`, so colour and depth flip together by construction and the whole rasterizer thinks y-up. `getData()` stays raw top-left for the Vulkan upload, so the display pipeline was untouched. Cost: Lesson 1–2 positional tests had to be re-anchored (see Modules). |
| Depth test placement | inside `drawTriangle`'s inner loop: `z > getDepth(px,py)` → `setDepth` + `setPixel` | The rasterizer owns the read-modify-write of the `Framebuffer`'s depth buffer. |
| Pixel sample point | **corner** — the rasterizer samples at integer coordinates, i.e. pixel centres sit *on* the integers | Simpler and matches Lesson 2; self-consistent throughout. Flagged, deliberately not changed: the usual convention puts the centre at `(x+0.5, y+0.5)`, so this is a half-pixel shift against how multisample patterns and texture sampling are normally specified. **Settle it when AA arrives.** |
| **Bounding-box rounding** *(Session 28)* | **clamp to `[0, width-1]` first, then a bare `static_cast<int>`** — no `floor`/`ceil` | The bbox is only a *conservative loop range*; the barycentric test is the coverage authority. A box that is too large costs a few rejected tests, one that is too small silently drops pixels. Truncation only shrinks the box for negative coordinates, and clamping before the cast removes that case entirely — so `static_cast<int>` **is** `floor` everywhere reachable. **Revisit at AA:** multi-sampling makes pixels whose centre lies outside still partially covered, so the box must grow half a pixel per side and `floor`/`ceil` becomes the minimum correct rule. |
| `cross` spelling | **RESOLVED** — reuse the existing templated `cross(Vec2,Vec2)`, which returns a scalar | Trivially satisfied once `RasterVertex` stores a `Vec2f` outright: there is no longer a `Vec3f` to drop a component from, so the ~5 longhand `.x`/`.y` extractions the earlier design needed do not exist and `tinymath::xy()` is unnecessary. |

## Implementation order (dependency-ordered) — all steps DONE
1. ~~**`tinymath` namespace move-commit**~~ — wrapped `Vec2` + all users, its own commit.
2. ~~**`Vec3<T>`**~~ — storage + member `+`/`-`/`*scalar`, free `cross`/`dot`. `tests/test_vec3.cpp`.
3. ~~**Projection** (free fn in `tinymath`) + **`io/ObjLoader`**~~ — both produce/consume `Vec3f`.
4. ~~**Triangle layer**~~ *(shipped as revised in Session 28)* — `primitives/Triangle.h` holds `RasterVertex`, `Triangle` and the frozen `Triangle2D`, structs only. `TriangleRasterizer.h` declares `namespace screen` with `BBox`, `BarycentricWeights` and the three functions, each carrying its contract doc comment; `TriangleRasterizer.cpp` defines them.
5. ~~**`drawTriangle` rewrite**~~ — `twiceSignedArea` once above the loop (zero → early return, negative → cull), bbox clamped then cast, then per pixel: `barycentricWeights` → `α,β,γ >= 0` → `z = α·a.depth+β·b.depth+γ·c.depth` → `z > getDepth` → `setDepth` + `setPixel`.
6. ~~**Square window (800×800)** + wire the model into the producer~~; visual check against the "before" picture passed; back-face culling added last and benchmarked; unit tests written.

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
**Responsibility:** the triangle value types — **structs only. No methods on `Triangle`, no namespace, no free functions in this file.**
**API:**
- `struct RasterVertex { tinymath::Vec2f coordinates; float depth; };` — a point in screen space with sub-pixel precision, plus its depth in `[0,1]` where **0 is far and 1 is near**. The split is the point: positions and depth are different kinds of number (concept 7), and no screen-space operation can reach a third spatial coordinate because none exists.
- `struct Triangle { RasterVertex a, b, c; };` — three of them, no qualifier (concept 7). This is the type the shipped rasterizer draws.
- `struct Triangle2D { tinymath::Vec2i a, b, c; };` — frozen Lesson-2 artifact, deprecated, retains `sortedByY()` / `getXBounds()` / `getYBounds()`. Reached only by `drawTriangleScanline` and `drawTriangle2D`.

**No geometry-side triangle type exists, and none is needed.** `io::Mesh` (`vector<Vec3f> vertices` + `vector<array<int,3>> faceIndices`) **is** the model-space representation, and it is the *indexed* one — a standalone geometry triangle would denormalize it and copy every vertex ~6× on a closed mesh. Lesson 5's face normal pulls three `Vec3f` out of the mesh by index, crosses two edges and normalizes: a free function over three vertices, no struct.

### `src/rasterizer/TriangleRasterizer.h/.cpp` (change)
`namespace screen` is **declared in this header and defined in this `.cpp`** — see the placement row in Design decisions. The namespace/filename mismatch is deliberate and temporary; it moves into `RasterizePrimitive`'s file when that is extracted.

**`namespace screen` API** — two return structs and three functions, all assuming already-projected coordinates:

- `struct BBox { float x_min, y_min, x_max, y_max; };` and `struct BarycentricWeights { float alpha, beta, gamma; };` — named members instead of `std::pair`/`Vec3f`/`std::tuple`; see the Design decisions rows. The pairing comment (`alpha` weights `a`, and so on) sits on the members themselves.

- `float twiceSignedArea(const Triangle& triangle)` — `cross(b-a, c-a)` on the **coordinates only, depth ignored**. Three facts the doc comment states, because none is visible from the signature:
  1. **xy-only / depth-ignored.** Moving a vertex in depth must not change the area or the facing.
  2. **Signed.** The sign is the winding, which is what back-face culling reads.
  3. **Twice** the geometric area — i.e. the *parallelogram* area, since the triangle is half of it. The factor is load-bearing: the edge weights carry the same factor, which is the only reason `α+β+γ = 1`. "Fixing" it with a `/2` silently doubles all three weights. It is never divided out because it cancels.

  Returns `0.0f` for a degenerate triangle — the caller's signal *not* to call `barycentricWeights`, which divides by it.

  Verified numerically (Session 27): the choice of shared vertex is irrelevant — `cross(b-a,c-a)`, `cross(c-b,a-b)`, `cross(a-c,b-c)` all agree. **Operand order flips the sign.** And `w0+w1+w2` equals `cross(b-a,c-a)` exactly, confirming it matches the weight convention.

- `BBox boundingBox(const Triangle& triangle)` — **deliberately raw floats, neither rounded nor clamped**; rounding and clamping are the caller's decision, and returning floats keeps that choice at the call site instead of baking a truncation rule into the primitive.

- `BarycentricWeights barycentricWeights(const Triangle& triangle, tinymath::Vec2f raster_point, float twice_signed_area)` — the doc comment states the α→`a` / β→`b` / γ→`c` pairing, the sum-to-1, the single-branch `>= 0` coverage test valid for both windings, why the area is passed in, and the non-zero precondition. Nothing in the type system enforces the precondition; a stale or mismatched area yields weights that are silently wrong rather than obviously broken.

**`drawTriangle(const Triangle&, Color, Framebuffer&, bool cull_back_faces = true)`** — `twiceSignedArea` once (zero → return, negative → cull when the flag is set), bbox **clamped to `[0, getWidth()-1]` / `[0, getHeight()-1]` then cast**, then per pixel: `barycentricWeights` → single-branch `α,β,γ >= 0` → `z = α·a.depth+β·b.depth+γ·c.depth` → `z > getDepth` → `setDepth` + `setPixel`. The clamp is a real optimisation: without it the loop walks the full bbox and lets `setPixel` silently reject out-of-range writes, so a mostly-off-screen triangle burns an entire inner loop doing nothing.

**Float truncation trap — resolved by ordering.** `static_cast<int>` rounds **toward zero**, i.e. `floor` for positive coordinates but `ceil` for negative ones. Clamping to `0` *before* the cast removes every negative input, so the bare cast is exactly `floor` on everything that reaches it. No explicit `floor`/`ceil` is needed today; both become necessary at AA (see the bounding-box row in Design decisions).

**Loop bounds are inclusive** (`<=`, not `<`). An exclusive bound drops the right column and the top row of every triangle.

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

### `tests/test_triangle_rasterizer.cpp` (change) — WRITTEN
The Lesson-2 `Triangle2D` cases stay; the Lesson-3 cases are appended below them behind a section banner. Shared fixtures: `frontFacing(da,db,dc)` and `backFacing(da,db,dc)` build the *same* three screen points in opposite winding order, so coverage is identical and only the sign of the area differs.

**Cases — `screen::` (reachable because they are declared in the header):**
- `twiceSignedArea` — known value (`+144`) on the right triangle with legs 12; **sign flips when two vertices are swapped** (the culling contract); `0` for collinear vertices; **unchanged when the depths vary** (proves the depth-ignored claim).
- `boundingBox` — a triangle spilling off the framebuffer on both axes with fractional coordinates, asserting the negative and out-of-range bounds come back **intact**. This is the case that proves the "raw, unrounded, unclamped" contract rather than merely describing it.
- `barycentricWeights` — at each vertex returns a single `1` and two `0`s (**the corner check that catches a permuted α/β/γ mapping** — see the bug below); ⅓,⅓,⅓ at the centroid; weights sum to 1 at four points, two inside and two outside; **all three non-negative for an interior point and not for an exterior one, under *both* windings** — the case that proves the single-branch coverage test.

**Cases — `drawTriangle`:**
- degenerate (collinear) triangle draws nothing;
- depth interpolates across the face: exact at a vertex, ⅓ at the centroid, monotonically decreasing away from the near vertex;
- **the nearer triangle wins regardless of draw order** — far-then-near and near-then-far both leave the near colour *and* the near depth. This is the property the pre-z-buffer renderer fails, and the reason both orders are asserted;
- a fragment behind the z-buffer leaves colour and depth untouched;
- culling drops a back-facing triangle, keeps it when the flag is off, and keeps a front-facing one either way;
- **culling does not change the image of front-facing geometry** — full-buffer comparison of a culled and an unculled render. Culling is a pure optimization, so this must be pixel-identical; the benchmark measures only the time difference.

> Per the Session-24/25 test-design lessons: pick **asymmetric** coordinates with every term non-zero. A symmetric case can be geometrically incapable of failing — it is what let the `Vec3::cross` typo and the y-flip bug both survive their first test.

### Two bugs these tests exist to catch (both were real)
1. **Rotated weight pairing** (caught in review, Session 28). The first `barycentricWeights` computed the three correct cross products but stored each under the **wrong name, rotated by one** — `alpha` got `cross(b-a, P-a)`, which is `0` at vertex `a`. Coverage, sum-to-1 and every drawn pixel would have been unchanged; only the depth gradient tilts. The vertex-collapse case is the only assertion here that fails on it. Rule: **the weight of a vertex is the area of the sub-triangle opposite it** — alpha from edge `b→c`, beta from `c→a`, gamma from `a→b`.
2. **`if (parallelogram_area = 0.0f)`** (found in Session 29 by an unexplained benchmark). A single `=` for `==`: the guard never fired, and worse, it **zeroed the area** before the cull test and the barycentric divisor. So nothing was ever culled, and every weight became `±inf` — whose signs happen to match the finite weights, so *coverage survived intact* and the picture still looked almost right. What actually died was the depth test (every fragment's `z` was `inf`, so every fragment won) and culling (`0 < 0` is false). The degenerate case and the draw-order case both fail on it immediately. **MSVC only reports this as C4706 at `/W4`** — the default `/W3` is silent.

## Open questions / carry-forward
- ~~**Scanline rung fate**~~ — **resolved**: kept, on `const Triangle2D&`, signature edit only.
- ~~**`cross` spelling** on `Vec3` verts~~ — **resolved**, then made moot: `RasterVertex` stores a `Vec2f` outright, so nothing drops a component.
- ~~**Pixel sample point**~~ — **resolved for now**: corner sampling, i.e. centres on the integers. Flagged as a half-pixel deviation from the usual convention; **settle it when AA arrives**.
- ~~**`screen::` + `RasterTriangle` redundancy**~~ — moot: the type is now plain `Triangle`.
- ~~**`Vec2` `.z`-dropping repetition** / `tinymath::xy()` helper~~ — moot for the same reason.
- **`Vec2` still has only `operator-`** (no `+`, no `*`) — the "retrofit opportunistically" decision still has not happened. Nothing in this lesson needed the rest; revisit when a lesson does.
- **`Triangle2D`'s fate.** Kept "for posterity" with a deprecation comment, reachable only by `drawTriangleScanline` and `drawTriangle2D`. Unresolved: git history is already the museum, and a deprecated struct in a live header tends to read as an option rather than a fossil.
- **Compiler warning level.** The `=`/`==` bug above is a one-character typo that silently disabled two features and was found by a benchmark anomaly, not by the build. `CMakeLists.txt` sets no explicit warning level — raise it (`/W4`, or `-Wall -Wextra`) so this class of bug is a build message.
- **`Framebuffer` has no comment recording the depth convention.** The "strip the placeholder wording" carry-forward turned out to be already done — there is no `placeholder` text anywhere in `src/`, and no `DEPTH_CLEAR` constant either, just a bare `0.0f` in three places (`Framebuffer.cpp:9`, `:45`, `:79`). The convention (0 = far, larger = nearer, `getDepth` out-of-bounds returns far) is now load-bearing and undocumented at the point of use.
- **Near-degenerate triangles:** `twice_signed_area` is float where Lesson 2's was exact `int`. A nearly-degenerate triangle gives a tiny denominator and enormous weights. Note it; no epsilon until it misbehaves.
- **Culling benchmark residual confounds.** The measured 2.02× is *slightly above* the theoretical ceiling for a 46% cull rate, because a culled triangle still pays for its `twiceSignedArea` call. The overshoot is harness bias, all of it favouring the second pass: no `framebuffer_.clear()` between the two runs (so pass 2 draws against a z-buffer pass 1 already filled and most of its fragments skip both writes), and pass 1 absorbing the cold-cache and first-touch page faults. Clear between passes and take second readings for an honest figure a little under 2×.
- **`Matrix4x4`** — introduced in Lesson 5 (camera); projection rebuilt on it then. Do **not** build it this lesson.
- **Deferred `Application` cleanups** (Lesson 1–2): dead `drawPoint()`/`drawTestPattern()`, `std::array<Vec2i,2>` → `Line`, stale bench `cout` labels — fold in opportunistically, non-blocking.
- ~~**Window** flips 800×600 → 800×800 as part of Lesson 3 setup.~~ **Done.**
- **Right-edge off-by-one in `orthographicProjection`** — `point_position.x == +1` maps to exactly `screen_width`, but the last valid column is `screen_width - 1`, so an extreme-edge vertex is silently dropped by `setPixel`'s bounds check. Same on y. Undecided: accept it (TinyRenderer does) vs. scale by `width - 1` vs. clamp. Revisit if the wireframe shows a missing edge.
- ~~**Assets are gitignored**~~ — `README.md` now documents where to obtain them. Still open underneath: paths resolve against the **current working directory**, not the executable, so the model is a manual copy inside the build directory and the app must be launched from there. `GraphicsPipeline.cpp:45` has the same CWD-relative bug for `shaders/display.spv`. A resolver is deferred to the end of the project (user's call); it must **not** be named `FileUtils.h` — that would shadow the engine's and break `readSpirv`.
- **`ObjLoader`'s face parser still assumes full `v/vt/vn` triads** — an OBJ with position-only or `v//vn` faces will not parse.
- **`testDrawMesh()` uses `diablo3_pose.obj`**; the lesson's reference asset is `african_head.obj` (the smaller mesh). Switch if a like-for-like comparison against the lesson's images matters.
