# Tiny Renderer SE

A software rasterizer built from scratch, following the [TinyRenderer](https://haqr.eu/tinyrenderer/) lecture series, displayed live inside a Vulkan window provided by the [Snake Engine](https://github.com/vfxsnake/vk_tutorial_se).

This is a learning project with two goals:

1. Implement every classic rasterization algorithm by hand — lines, triangles, z-buffering, texture mapping, lighting — entirely on the CPU, no graphics API shortcuts.
2. Bridge that CPU output into a live interactive window so the result can be seen in real time, not just written to a file.

A second phase (planned) will port the rasterizer algorithms to GPU compute shaders, using the CPU version as the reference implementation.

---

## Current status

**As of 2026-08-30 — Lesson 6 (Shading): the shader path runs end to end and is on screen; the second concrete shader is half written.** `src/rasterizer/shaders/AbstractShader.h` is written — a virtual destructor plus two pure virtuals, `vertex(face_index, vertex_index)` returning a clip-space `Vec4f` and `fragment(weights, out_color)` returning `false` to discard — and it carries **no data members**, because the varyings live on the concrete shaders. It is a `class` rather than a `struct` despite the extra `public:`, on a hazard worth knowing: **the inheritance default follows the keyword too**, so `class FaceShader : AbstractShader` would inherit *privately* and `FaceShader&` would silently fail to convert to `AbstractShader&`, with the error surfacing at the call site rather than at the declaration. The second parameter is the **corner number within the face, not a global vertex id**, because the varyings are a three-slot array — `vertex()` writes `varying_[n]` and `fragment()` reads slot 0 by `alpha`, 1 by `beta`, 2 by `gamma` — and because it is the only pair that can do the lookup, `faceIndices[face_index * 3 + n]` and `faceNormalIndices[face_index * 3 + n]` being independent (which is also why hard edges work). Writing it first required an unplanned refactor: **`screen::BarycentricWeights` lived inside `TriangleRasterizer.h`**, which is itself about to take an `AbstractShader&`, so the two headers wanted each other. Rather than forward-declare around it, the whole `screen` vocabulary was lifted into **`src/rasterizer/ScreenSpace.h/.cpp`** — `BBox`, `BarycentricWeights`, `twiceSignedArea`, `boundingBox`, `barycentricWeights`, moved verbatim — so both sides depend on the vocabulary instead of on each other; the whole namespace went rather than only the one type, because splitting a namespace across two headers is worse than either whole option. 

`drawTriangleWithShader` now exists beside the untouched `drawTriangle`, taking `const std::array<Vec4f, 3>& clip_positions`, an `AbstractShader&`, the framebuffer and a cull flag — and its **body is complete**. The shader is *forward-declared* rather than included, so `TriangleRasterizer.h` never names `shaders/` and the header cycle is closed for good. `std::array<Vec4f, 3>` rather than a `Triangle`, because `Triangle` holds `RasterVertex` (`Vec2f` + depth) and is therefore screen space by construction — the divide has happened and `w` is gone — while clip space still needs all four components. The body reuses `drawTriangle`'s skeleton verbatim (degenerate check, back-face cull, bounding box, barycentric coverage test, interpolated depth) with a prologue in front and one substitution inside: the prologue turns the three clip positions into a screen-space `Triangle`, and the constant colour is replaced by a per-pixel `shader.fragment(weights, out_color)` whose `false` return **discards** — skipping the colour *and* the depth write, so an invisible fragment cannot occlude what is behind it.

The prologue's ordering was argued rather than copied, and the argument is the reusable part. Written the obvious way — `toVec3(viewport_matrix * clip)` — it is numerically correct, because the viewport's bottom row is `(0,0,0,1)`, so `(M*v).w == v.w` and the divide still cancels the `half_width*w` translate term. It was rewritten anyway to divide **first** (`toVec3` → NDC), promote back with `toVec4` (`w` is now literally 1), then apply the viewport and read the components with no second divide. The reason is about where the divide is *attached*, not about the arithmetic: `toVec3(M * v)` divides by `(M*v).w`, so if `M` ever had a live bottom row — say `(0,0,-1/f,0)` — that becomes `-v.z/f`, a number invented from `v.z`, and the line keeps compiling while producing garbage. **The divide belongs immediately after the matrix that manufactured the `w`**; attaching it to the viewport lands on the right number only by accident of the viewport being affine. Cost of the safer order is one extra `toVec4` per vertex, register work only. Its one real forfeit: dividing first throws away each vertex's `w`, which is exactly what perspective-correct interpolation will want in Lesson 7.

The pixel loop tests depth **before** calling the shader — **early-z**, derived from the cost asymmetry: the depth test is three multiplies, an add, a buffer read and a compare, while a fragment call is open-ended (texture fetches, normalize, dots, specular pow), so only surviving pixels should pay for it. This is a *toggle* on real hardware rather than an always-on optimisation, because it is valid only while the shader cannot write depth — GPUs disable early-z the moment a shader writes `gl_FragDepth`. `AbstractShader` offers no way to do that, so this path is safe by construction.

The full space chain, with this project's own numbers read off `Projection.cpp`: model → world → view (`lookAt`) → clip (`perspective`) → **NDC** (divide by `w`; x, y in [-1, 1], z in [-1, 1]) → screen (`viewport`, which lands depth in [0, 1]). *NDC* = **N**ormalized **D**evice **C**oordinates — normalized because the ranges are fixed and unitless, *device* because it is the last space before the output device's pixel dimensions get involved, which is why nothing in it distinguishes 800×800 from 4K and why clipping belongs here rather than in pixels.

`RandomShader` is the first concrete `AbstractShader` — a constant/unlit *model* at primitive *rate*, one arbitrary colour per triangle, no lighting arithmetic anywhere. It exists to isolate the plumbing from the shading: five new mechanisms land at once with the shader path (`vertex()` called three times per face with the right indices, clip positions coming back correct, the rasterizer's new divide, its new viewport, `fragment()`'s return reaching the framebuffer), and a first shader that also lit the surface would give a wrong picture six suspects instead of five. Random rather than one constant colour, because a single colour renders the mesh as a featureless silhouette where a per-triangle geometry error hides completely — and the target image is already known, being the confetti render from Lesson 3. Colour comes from a fixed-seed `std::mt19937`: repeatable across runs, which is what makes A/B comparison possible at all. Hashing the face index was the stronger diagnostic (it binds the colour to the *face* rather than to *draw order*, so two renders stay comparable face-by-face even if traversal changes) but a shader named for randomness that contains none is misnamed, and the picture is identical either way.

Writing its includes surfaced a layering mistake: **`Mesh` lived in `src/io/ObjLoader.h`, inside `namespace io`** — the data type bundled with the thing that produces it, so a shader had to include the loader, `<string>` and `loadObj` to hold one. It now lives in **`src/geometry/Mesh.h`** in the global namespace: `io` means input/output, and a mesh is geometry that merely *arrives* that way. Deliberately **not** filed under `rasterizer/primitives/` beside `Triangle.h`, tempting as the sibling was — `Triangle` there is *screen-space raster* data, while `Mesh` is model-space geometry the Phase 2 GPU path will consume too, so filing it under `rasterizer/` would bind renderer-agnostic data to the CPU implementation. The namespace was dropped rather than renamed, matching the project's existing split: `tinymath::`, `io::` and `screen::` mark *behaviour* groupings, while the data types (`Triangle`, `Color`, `Framebuffer`) are global.

The shader's storage choices are asymmetric on purpose, and the asymmetry is the transferable part. `mesh_` is a `const Mesh*` from a `const Mesh&` parameter — the reference says "required, non-null" at the call site, the pointer keeps the member assignable. `transform_` takes a `const Matrix4x4&` parameter but is stored **by value**, because a reference member here would dangle rather than merely restrict: the call site builds `perspective * lookAt` as a temporary that dies at the end of the full expression. Sixty-four bytes copied once per shader against megabytes of mesh that genuinely outlive it. This is also the boundary of what a forward declaration can do — `struct Mesh;` suffices for a pointer, where the compiler needs only the *name*, and would not suffice for `transform_`, where it needs the *layout*; the same line divides base classes and `sizeof` from parameters and returns.

`RandomShader` now has a call site — `Application::testDrawMeshRandomShader()` — and the whole path has executed for the first time: three sessions of plumbing written blind, and the confetti render came back matching Lesson 3's on the first run, same silhouette, same framing, no seams between faces. That match is the load-bearing result, because it is what proves moving `viewport` **out** of the call site and **into** `drawTriangleWithShader` was correct. The call site composes `perspective * lookAt` only; the rasterizer owns the viewport now, so a shader never has to know the framebuffer's dimensions. The face loop fills a `std::array<Vec4f, 3>` from three separate `vertex()` calls, and **the order of those three statements is load-bearing** — the varying is written by side effect on a chosen call (`RandomShader` rolls its colour on corner 0, `FaceShader` computes its normal on corner 2, needing all three corners for two edges), so a braced initializer whose evaluation order C++ does not pin would be a real bug rather than a style choice.

`FaceShader` is next and is partly written — flat shading, one lighting evaluation per face, whose exit condition is stricter than confetti: it must reproduce `testDrawMeshMatrixLightWorldSpace()`'s grey render *pixel for pixel*, which is why the random colours were kept in a separate class rather than made a flag on this one. It deliberately computes its normal by `cross` on the triangle's own edges rather than reading the OBJ's `vn` data, for a reason worth stating: **those file normals are per-*vertex* normals** — smoothed across adjacent faces — so using them at face rate would mean arbitrarily picking one or averaging the three, giving neither flat nor smooth shading. The cross product is the face's true geometric normal. The file normals are exactly what `GouraudShader` consumes at the next step, one per corner blended by the barycentric weights, so flat-vs-Gouraud stays a clean one-axis A/B: same light, same base colour, only the normal source and the shading rate change.

The lesson kick-off protocol changed at the start of this lesson: **spike first, document second.** Concepts are triaged into *show it* (has a visible failure mode — spike and break it, don't pre-discuss), *measure it* (answerable by running numbers over the real mesh) and *discuss it* (genuinely invisible — design forks and effects that only manifest later). The plan doc now records the design the spike earned rather than blocking the first pixels.

Overall arc: work through the TinyRenderer lessons in order (CPU rasterizer, by hand) until an OBJ model head renders shaded in the live window, then a **Phase 2** ports the algorithms to GPU compute shaders using the CPU version as reference.

- **Phase 0 — Display pipeline: complete.** CPU framebuffer uploads to a Vulkan texture each frame and renders live in the window (fullscreen triangle, dynamic rendering). Tagged `phase0-display-complete` — this is the renderer-agnostic seed reused by a separate planned **Ray Tracing in One Weekend** project.
- **Lesson 1 — Line drawing: complete.** Naive → accumulator → integer-Bresenham ladder, benchmarked and unit-tested (differential test across all three rungs).
- **Lesson 2 — Triangle rasterization: complete.** Two rungs benchmarked (scanline vs bounding-box + barycentric); barycentric shipped as `drawTriangle` — slower on scalar CPU (~2.4–2.9×) but the pixel-independent, GPU-portable path. `Vec2` gained `cross`/`operator-`; unit-tested.
- **Lesson 3 — Hidden face removal (z-buffer): complete.** The lesson pulled in everything the model needed before any depth code existed: the `tinymath` namespace adopted across the math and rasterizer layers; `Vec3<T>` (+ `dot`/`cross`); a minimal OBJ loader (`io::loadObj` → vertices + face indices); orthographic projection (`tinymath::orthographicProjection`); the framebuffer origin moved to bottom-left so the rasterizer works y-up; a square 800×800 window. A **wireframe checkpoint** validated the loader, the projection and the framebuffer orientation independently, then the model rendered **solid** through the Lesson-2 barycentric rasterizer with one random colour per face — faces overlapping in file order, no depth ordering at all, the "before" picture the rest of the lesson removes.

  The screen-space layer then shipped. `RasterVertex` pairs a `Vec2f` screen position with a `float depth` and `Triangle` holds three of them — the split records that **after projection, depth is an interpolated attribute rather than a third spatial axis**, which structurally prevents crossing two projected vertices to get a normal. The operations `twiceSignedArea`, `boundingBox` and `barycentricWeights` are free functions in a `screen::` namespace, the name recording that they assume already-projected coordinates. Screen coordinates stay in floating point rather than snapping to whole pixels, preserving the sub-pixel precision that coverage-based anti-aliasing will need later.

  `drawTriangle` interpolates depth from the barycentric weights and runs a per-pixel depth test (convention: clear `0.0f` = far, keep larger z), which **resolves the model's front and back surfaces correctly** — the lesson's exit condition. Back-face culling ships as an optional flag driven by the sign of the signed area and measures **2.02×** on 5022 triangles (11.71 ms → 5.79 ms, Release; 2314 culled), verified to leave the image pixel-identical since culling is a pure optimization. 44 unit tests cover the three `screen::` functions, depth interpolation, the order-independence of the depth test, and culling.

- **Lesson 4 — Naive camera handling: complete.** The fixed head-on orthographic view became a movable one. Two free functions in `tinymath` — `rotateY` in a new `math/Transform.h/.cpp`, `perspectiveZDivide` beside `orthographicProjection` in `math/Projection.h/.cpp` — composed as `rotateY → perspectiveZDivide → orthographicProjection`. The file split is deliberate: it is the **model-view / projection separation showing up as a file boundary**. Rotation stayed a free function rather than a `Vec3` method, because rotation is a map *applied to* a vector, not a property *of* one.

  The lesson's substance came from breaking the spike on purpose. **Running the divide before the rotation** turns it from a projection into a non-rigid deformation in object space — diablo's tail, which points backwards, gets physically shortened along its own axis and then swung sideways into view; the tell is that the distortion follows *anatomy* rather than viewing direction. **Sweeping the eye distance** established that `c` is camera *distance*, not focal length (focal length alone changes no geometry; the screen sits at `z = 0` through the model, so `c` is eye-to-subject and eye-to-screen at once, and moving it is a dolly). It also inverted an assumption: since `k = 1/(1 − z/c)` is `< 1` behind the pivot plane, **the perspective divide compresses the far side and therefore protects the depth invariant — orthographic is the worst case for depth, not the safest.** The lesson's own homework bug (an 8-bit grayscale z-buffer wrapping at `1.17 × 255`) cannot reach a float depth buffer; the remaining hole mechanism is the `0.0f` clear, which needs a mesh deeper than the `[−1,1]` box that `orthographicProjection` assumes — **a property of the model, not the camera.**

Per-lesson design docs live in [`docs/lessons/`](docs/lessons/). Lesson 4 has none by decision — it closed on screen and was superseded by Lesson 5, so the write-up would document history rather than the current design.

---

## Architecture

```
tiny_render_se/
├── src/
│   ├── main.cpp                  entry point
│   ├── Application.h/.cpp        window lifecycle, owns display pipeline + rasterizer
│   ├── math/                     hand-written math primitives (Vec2/3/4, Matrix4x4, ...)
│   ├── rasterizer/               software rasterizer (Framebuffer, line, triangle, ...)
│   └── display/                  Vulkan pipeline: uploads CPU framebuffer to GPU each frame
├── tests/                        Catch2 unit tests for math + rasterizer
├── models/                       OBJ models and textures
└── engine/vk_tutorial_se/        git submodule — Snake Engine (Vulkan context + swap chain)
```

The **display pipeline** is intentionally thin: each frame it copies the CPU-rendered pixel buffer into a staging buffer, uploads it to a GPU texture, and draws a fullscreen quad. No vertex buffers, no UBO, no depth attachment — just the CPU image on screen.

The **math layer** is written from scratch (no GLM). Every type (`Vec2<T>`, `Vec3<T>`, `Vec4<T>`, `Matrix4x4`) is header-only and unit-tested before use.

---

## Dependencies

| Dependency | How acquired |
|------------|-------------|
| Vulkan SDK | System install (LunarG) |
| GLFW 3.4 | CMake FetchContent |
| Vulkan-Hpp | CMake FetchContent |
| stb | CMake FetchContent |
| Catch2 | CMake FetchContent (added with first test) |
| Snake Engine | Git submodule (`engine/vk_tutorial_se/`) |

---

## Build

```bash
# Clone with submodules
git clone --recurse-submodules <repo-url>
cd tiny_render_se

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run — from the repository root, see note below
./build/TinyRendererSE
```

Requires a Vulkan-capable GPU and the LunarG Vulkan SDK installed.

### Models

Model and texture assets are **not tracked in this repository** (`.obj`, `.tga` and `.png` are gitignored — they are large binaries owned by the upstream lesson series). From Lesson 3 onward the renderer needs them, and will throw on startup if they are missing.

```bash
# From the repository root
git clone --depth 1 https://github.com/ssloy/tinyrenderer /tmp/tinyrenderer
cp -r /tmp/tinyrenderer/obj models/
```

This yields `models/obj/african_head/african_head.obj` and friends.

> Asset paths are resolved against the **current working directory**, not the executable — run the binary from the repository root, or the model will not be found.

---

## Lessons

The project follows the TinyRenderer lesson sequence. Each lesson adds one or more modules to `src/rasterizer/` and a corresponding test file.

| # | Topic | Status |
|---|-------|--------|
| 0 | Display pipeline (Vulkan window + CPU framebuffer upload) | Complete |
| 1 | Line drawing (Bresenham) | Complete |
| 2 | Triangle rasterization | Complete |
| 3 | Hidden face removal (z-buffer) | Complete |
| 4 | Naive camera handling (rotation + central projection) | Complete (lesson doc declined) |
| 5 | Better camera | Complete |
| 6 | Shading | In progress |
| 7 | More data! (textures, normal & specular maps) | Planned |
| 8 | Tangent space | Planned |
| 9 | Shadow mapping | Planned |
| 10 | Ambient occlusion | Planned |
| 11 | Toon shading (bonus) | Planned |
| — | Phase 2: port the rasterizer to GPU compute shaders | Planned |

> Lesson numbering follows the source series, with one deviation: the series splits "Triangle rasterization" and "Barycentric coordinates" into two lessons, which this project covered together as Lesson 2.

---

## License

MIT


## Build Commands:

- WSL: 
    cmake -S . -B build_wsl -G Ninja  
    cmake --build build_wsl

- Windows: 
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    
    cmake --build build --config Debug 
    or
    cmake --build build --config Release
    
    cd build
    Debug\TinyRendererSE.exe

