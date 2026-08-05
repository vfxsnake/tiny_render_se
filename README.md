# Tiny Renderer SE

A software rasterizer built from scratch, following the [TinyRenderer](https://haqr.eu/tinyrenderer/) lecture series, displayed live inside a Vulkan window provided by the [Snake Engine](https://github.com/vfxsnake/vk_tutorial_se).

This is a learning project with two goals:

1. Implement every classic rasterization algorithm by hand — lines, triangles, z-buffering, texture mapping, lighting — entirely on the CPU, no graphics API shortcuts.
2. Bridge that CPU output into a live interactive window so the result can be seen in real time, not just written to a file.

A second phase (planned) will port the rasterizer algorithms to GPU compute shaders, using the CPU version as the reference implementation.

---

## Current status

**As of 2026-08-05 — Lesson 4 (Naive camera handling) complete on screen; Lesson 5 (Better camera) is next. The model rotates about the y-axis under central projection: `tinymath::rotateY` (new `math/Transform.h`) and `tinymath::perspectiveZDivide` (added to `math/Projection.h`) compose as `orthographicProjection(perspectiveZDivide(rotateY(v)))`, with the near side visibly larger than the far side in the live window. The stage order and the near/far depth behaviour were both diagnosed by deliberately breaking the spike rather than by argument. `Matrix3x3` was considered and **cancelled** — Lesson 5 needs the 4×4 anyway, so the hand-rolled perspective divide will collapse into the matrix pipeline there. Still spike-quality: free functions, hardcoded 45° and eye distance 3, and no lesson doc for this one. Lesson 3 (Hidden face removal / z-buffer) is complete: 2.02× back-face culling on 5022 triangles, 44 unit tests.**

The lesson kick-off protocol changed this session: **spike first, document second.** Concepts are triaged into *show it* (has a visible failure mode — spike and break it, don't pre-discuss), *measure it* (answerable by running numbers over the real mesh) and *discuss it* (genuinely invisible — design forks and effects that only manifest later). The plan doc now records the design the spike earned rather than blocking the first pixels.

Overall arc: work through the TinyRenderer lessons in order (CPU rasterizer, by hand) until an OBJ model head renders shaded in the live window, then a **Phase 2** ports the algorithms to GPU compute shaders using the CPU version as reference.

- **Phase 0 — Display pipeline: complete.** CPU framebuffer uploads to a Vulkan texture each frame and renders live in the window (fullscreen triangle, dynamic rendering). Tagged `phase0-display-complete` — this is the renderer-agnostic seed reused by a separate planned **Ray Tracing in One Weekend** project.
- **Lesson 1 — Line drawing: complete.** Naive → accumulator → integer-Bresenham ladder, benchmarked and unit-tested (differential test across all three rungs).
- **Lesson 2 — Triangle rasterization: complete.** Two rungs benchmarked (scanline vs bounding-box + barycentric); barycentric shipped as `drawTriangle` — slower on scalar CPU (~2.4–2.9×) but the pixel-independent, GPU-portable path. `Vec2` gained `cross`/`operator-`; unit-tested.
- **Lesson 3 — Hidden face removal (z-buffer): complete.** The lesson pulled in everything the model needed before any depth code existed: the `tinymath` namespace adopted across the math and rasterizer layers; `Vec3<T>` (+ `dot`/`cross`); a minimal OBJ loader (`io::loadObj` → vertices + face indices); orthographic projection (`tinymath::orthographicProjection`); the framebuffer origin moved to bottom-left so the rasterizer works y-up; a square 800×800 window. A **wireframe checkpoint** validated the loader, the projection and the framebuffer orientation independently, then the model rendered **solid** through the Lesson-2 barycentric rasterizer with one random colour per face — faces overlapping in file order, no depth ordering at all, the "before" picture the rest of the lesson removes.

  The screen-space layer then shipped. `RasterVertex` pairs a `Vec2f` screen position with a `float depth` and `Triangle` holds three of them — the split records that **after projection, depth is an interpolated attribute rather than a third spatial axis**, which structurally prevents crossing two projected vertices to get a normal. The operations `twiceSignedArea`, `boundingBox` and `barycentricWeights` are free functions in a `screen::` namespace, the name recording that they assume already-projected coordinates. Screen coordinates stay in floating point rather than snapping to whole pixels, preserving the sub-pixel precision that coverage-based anti-aliasing will need later.

  `drawTriangle` interpolates depth from the barycentric weights and runs a per-pixel depth test (convention: clear `0.0f` = far, keep larger z), which **resolves the model's front and back surfaces correctly** — the lesson's exit condition. Back-face culling ships as an optional flag driven by the sign of the signed area and measures **2.02×** on 5022 triangles (11.71 ms → 5.79 ms, Release; 2314 culled), verified to leave the image pixel-identical since culling is a pure optimization. 44 unit tests cover the three `screen::` functions, depth interpolation, the order-independence of the depth test, and culling.

- **Lesson 4 — Naive camera handling: complete.** The fixed head-on orthographic view became a movable one. Two free functions in `tinymath` — `rotateY` in a new `math/Transform.h/.cpp`, `perspectiveZDivide` beside `orthographicProjection` in `math/Projection.h/.cpp` — composed as `rotateY → perspectiveZDivide → orthographicProjection`. The file split is deliberate: it is the **model-view / projection separation showing up as a file boundary**. Rotation stayed a free function rather than a `Vec3` method, because rotation is a map *applied to* a vector, not a property *of* one.

  The lesson's substance came from breaking the spike on purpose. **Running the divide before the rotation** turns it from a projection into a non-rigid deformation in object space — diablo's tail, which points backwards, gets physically shortened along its own axis and then swung sideways into view; the tell is that the distortion follows *anatomy* rather than viewing direction. **Sweeping the eye distance** established that `c` is camera *distance*, not focal length (focal length alone changes no geometry; the screen sits at `z = 0` through the model, so `c` is eye-to-subject and eye-to-screen at once, and moving it is a dolly). It also inverted an assumption: since `k = 1/(1 − z/c)` is `< 1` behind the pivot plane, **the perspective divide compresses the far side and therefore protects the depth invariant — orthographic is the worst case for depth, not the safest.** The lesson's own homework bug (an 8-bit grayscale z-buffer wrapping at `1.17 × 255`) cannot reach a float depth buffer; the remaining hole mechanism is the `0.0f` clear, which needs a mesh deeper than the `[−1,1]` box that `orthographicProjection` assumes — **a property of the model, not the camera.**

Per-lesson design docs live in [`docs/lessons/`](docs/lessons/) — Lesson 4 is the one gap, closed on screen but never written up.

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
| 4 | Naive camera handling (rotation + central projection) | Complete (no lesson doc) |
| 5 | Better camera | Next |
| 6 | Shading | Planned |
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

