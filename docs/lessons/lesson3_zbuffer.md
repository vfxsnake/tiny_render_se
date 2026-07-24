# Lesson 3 — Hidden Faces Removal (Z-Buffer)

**Source:** https://haqr.eu/tinyrenderer/z-buffer/

## Goal
Render the OBJ head as a solid, per-pixel-occluded surface — near triangles hide far ones so no back-side geometry bleeds through the front.

## Exit condition
The window shows the head model filled with triangles (flat / per-triangle color for now), depth-correct: the nose sits in front of the cheeks, no far-side face punches through. Plus a depth-test unit case and whatever property/differential tests the depth-aware fill needs. **Status: PLANNED.**

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

## Design decisions
| Decision | Choice | Reason |
|----------|--------|--------|
| Screen-space vertex | **`Vec3f`** (not `Vec2i` + separate z) | `Vec3` is needed anyway for world vertices; the integer-edge optimization is already dead (barycentric forces float division regardless). Float geometry, integer pixel grid. |
| **`Triangle` shape** | **Option A** — change the struct in place: `struct Triangle { Vec3f a, b, c; }` (not a template, not a second type) | `.x/.y` feed 2D ops (bbox, edge functions, sort); `.z` feeds depth. One type, no premature abstraction. |
| `sortedByY()` | return shifts `array<Vec2i,3>` → **`array<Vec3f,3>`** | Sorts on `.y`; z rides along — exactly what post-sort depth interpolation wants. |
| Lesson-2 2D path | `test_triangle_rasterizer.cpp` + `drawTriangleScanline` migrate to `Vec3f` (z=0 for pure-2D cases) | Decide at implementation whether the scanline rung/tests are **retired** now that `drawTriangle` ships — the scanline path has no future role. |
| `Vec3<T>` API | member `+`, `-`, `*scalar`; **`cross`/`dot` as free functions**; Hadamard `*` **deferred** to shading | Consistent with `Vec2` (free `cross`); no redundant member/free duplication; component-wise multiply has no consumer until Lesson 8–9. Left-scalar `scalar*vec` deferred until needed. |
| `Vec2` retrofit | add ops to `Vec2` opportunistically to match `Vec3` as lessons touch them | Keep the two types from drifting into different mental models. |
| **Namespace** | **`tinymath`** (lowercase); adopt in **one dedicated move-commit BEFORE Lesson 3 code** | The deferred "math namespace" move. Blast radius: `Vec2.h`, `test_vec2.cpp`, `LineDrawer.{h,cpp}`, `TriangleRasterizer.{h,cpp}`, `Triangle.h`. `math` isn't actually taken by std — `tinymath` chosen for clarity. |
| OBJ loader | `src/io/ObjLoader.*`, free `loadObj(path)` → minimal mesh (`vector<Vec3f>` verts + `vector<array<int,3>>` faces); parse only `v` and the position index of each `f` group, ignore `vt`/`vn` | Asset I/O, not math or rasterization. Positions are all this lesson needs; UV/normal storage arrives with textures/shading. |
| Projection | **free function in `tinymath`**, orthographic scale/shift now; **`Matrix4x4`-backed in Lesson 5** | Projection is coordinate-space math (its home is `tinymath` from day one); but a matrix to express a scale-and-shift is premature — build the matrix when the camera lesson demands it. |
| Depth test placement | inside `drawTriangle`'s inner loop: `z > getDepth(px,py)` → `setDepth` + `setPixel` | The rasterizer owns the read-modify-write of the `Framebuffer`'s depth buffer. |
| Pixel sample point | corner (`px,py`) vs center (`px+0.5,py+0.5`) — **decide when writing `drawTriangle`** | Center is more correct for coverage/tie-breaking; corner is simpler and matches Lesson 2. |
| `cross` spelling on `Vec3` verts | pull a `Vec2` from `.x/.y`, or add a 2D-cross helper reading `.x/.y` — **decide when writing `drawTriangle`** | Minor ergonomics; the 2D signed area only uses x,y. |

## Implementation order (dependency-ordered)
1. **`tinymath` namespace move-commit** — wrap `Vec2` + all users. Cleanup on existing code first, its own commit.
2. **`Vec3<T>`** — storage + member `+`/`-`/`*scalar`, free `cross`/`dot`. `tests/test_vec3.cpp`.
3. **Projection** (free fn in `tinymath`) + **`io/ObjLoader`** — both produce/consume `Vec3f`.
4. **`Triangle` → `Vec3f`** (Option A); migrate/retire the scanline path + its tests.
5. **`drawTriangle` rewrite** — float edge functions + sign reject → weights + z-interp + depth test in the inner loop.
6. **Square window (800×800)** + wire the head into the producer; visual check; tests. Add real back-face culling last (same image, faster) once the head renders correctly.

## Modules

### `src/math/Vec3.h` (new)
**Responsibility:** 3D vector — world vertex and (post-projection) screen vertex with depth.
**API:**
- `struct Vec3<T> { T x, y, z; };` templated; `using Vec3f = Vec3<float>;`
- members `operator+`, `operator-`, `operator*(T scalar)`
- free `T dot(Vec3<T>, Vec3<T>)`, free `Vec3<T> cross(Vec3<T>, Vec3<T>)`

### `src/math/Vec2.h` (move into `tinymath`, retrofit)
**Responsibility:** existing 2D vector, now namespaced; op set kept in step with `Vec3` as needed.

### `src/math/Projection.h` (or similar, in `tinymath`)
**API:**
- `Vec3f project(Vec3f v, int width, int height, float depth_scale)` — orthographic `(v+1)·scale/2` remap; z kept ≥ 0. Matrix-backed in Lesson 5.

### `src/io/ObjLoader.h/.cpp` (new)
**API:**
- `Mesh loadObj(std::string const& path)` — `Mesh { std::vector<Vec3f> vertices; std::vector<std::array<int,3>> faces; }`. Parse `v` lines and the first (position) index of each `f` group; skip `vt`/`vn`.

### `src/rasterizer/primitives/Triangle.h` (change)
**API:**
- `struct Triangle { Vec3f a, b, c; };`
- `std::array<Vec3f,3> sortedByY() const;` (z rides along)
- `getXBounds()` / `getYBounds()` now over `Vec3f.x/.y`

### `src/rasterizer/TriangleRasterizer.h/.cpp` (change)
**API:**
- `void drawTriangle(Triangle t, Color color, Framebuffer& frame_buffer)` — bbox → per-pixel sign reject → weights + `z = α·a.z+β·b.z+γ·c.z` → depth test → `setDepth`/`setPixel`.
- `drawTriangleScanline` — migrate to `Vec3f` or retire (decide at implementation).

### `tests/test_vec3.cpp` (new)
**Cases:** `+`/`-`/`*scalar`; `dot` known values; `cross` known values, anti-commutativity, parallel → zero.

### `tests/test_framebuffer.cpp` (extend)
**Cases:** depth clear = `0.0f`; `setDepth`/`getDepth` round-trip; the `z > getDepth` keep/discard decision.

### `tests/test_triangle_rasterizer.cpp` (change)
**Cases:** depth-aware fill writes both color and depth for interior pixels; a nearer triangle overwrites a farther one at a shared pixel; a farther one does not; degenerate (zero-area) draws nothing.

## Open questions / carry-forward
- **Scanline rung fate** — retire vs. migrate to `Vec3f`; decide at implementation (it has no shipping role).
- **Pixel sample point** (corner vs center) and **`cross` spelling** on `Vec3` verts — decide when writing `drawTriangle`.
- **`Matrix4x4`** — introduced in Lesson 5 (camera); projection rebuilt on it then. Do **not** build it this lesson.
- **Deferred `Application` cleanups** (Lesson 1–2): dead `drawPoint()`/`drawTestPattern()`, `std::array<Vec2i,2>` → `Line`, stale bench `cout` labels — fold in opportunistically, non-blocking.
- **Window** flips 800×600 → 800×800 as part of Lesson 3 setup.
