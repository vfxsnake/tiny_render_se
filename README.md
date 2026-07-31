# Tiny Renderer SE

A software rasterizer built from scratch, following the [TinyRenderer](https://haqr.eu/tinyrenderer/) lecture series, displayed live inside a Vulkan window provided by the [Snake Engine](https://github.com/vfxsnake/vk_tutorial_se).

This is a learning project with two goals:

1. Implement every classic rasterization algorithm by hand — lines, triangles, z-buffering, texture mapping, lighting — entirely on the CPU, no graphics API shortcuts.
2. Bridge that CPU output into a live interactive window so the result can be seen in real time, not just written to a file.

A second phase (planned) will port the rasterizer algorithms to GPU compute shaders, using the CPU version as the reference implementation.

---

## Current status

**As of 2026-07-31 — Lesson 3 (Hidden face removal / z-buffer) all but complete: the depth-buffered rasterizer works — an OBJ model renders solid in the live window with faces resolving in correct depth order per pixel, the artifacts from the "before" picture gone. Back-face culling is in but its measured saving looks too small, so the cull rate needs verifying; unit tests for the new screen-space layer are the last piece.**

Overall arc: work through the TinyRenderer lessons in order (CPU rasterizer, by hand) until an OBJ model head renders shaded in the live window, then a **Phase 2** ports the algorithms to GPU compute shaders using the CPU version as reference.

- **Phase 0 — Display pipeline: complete.** CPU framebuffer uploads to a Vulkan texture each frame and renders live in the window (fullscreen triangle, dynamic rendering). Tagged `phase0-display-complete` — this is the renderer-agnostic seed reused by a separate planned **Ray Tracing in One Weekend** project.
- **Lesson 1 — Line drawing: complete.** Naive → accumulator → integer-Bresenham ladder, benchmarked and unit-tested (differential test across all three rungs).
- **Lesson 2 — Triangle rasterization: complete.** Two rungs benchmarked (scanline vs bounding-box + barycentric); barycentric shipped as `drawTriangle` — slower on scalar CPU (~2.4–2.9×) but the pixel-independent, GPU-portable path. `Vec2` gained `cross`/`operator-`; unit-tested.
- **Lesson 3 — Hidden face removal (z-buffer): in progress.** Kick-off + design complete, plan doc written. Done so far: the `tinymath` namespace adopted across the math and rasterizer layers; `Vec3<T>` (+ `dot`/`cross`) written and unit-tested; a minimal OBJ loader (`io::loadObj` → vertices + face indices); orthographic projection (`tinymath::orthographicProjection`); the framebuffer origin moved to bottom-left so the rasterizer works y-up (`Framebuffer::getPixel` added, `Color` gained `operator==`, 30 tests green); window now square at 800×800. **Wireframe checkpoint passed** — an OBJ model loads, projects and draws as a live wireframe in the window using only the Lesson-1 line drawer, validating the loader, the projection and the framebuffer orientation independently before any depth code exists. The model then renders **solid** through the Lesson-2 barycentric rasterizer, one random colour per face — showing faces overlapping in file order with no depth ordering at all, which is the "before" picture the rest of the lesson fixes. The screen-space triangle layer then shipped: `RasterVertex` pairs a `Vec2f` screen position with a `float depth`, `Triangle` holds three of them, and the operations — `twiceSignedArea`, `boundingBox`, `barycentricWeights` — are free functions in a `screen::` namespace, the name recording that they assume already-projected coordinates. Keeping screen coordinates in floating point (rather than snapping to whole pixels) preserves the sub-pixel precision that coverage-based anti-aliasing will need later. `drawTriangle` now interpolates depth from the barycentric weights and does a per-pixel depth test (convention: clear `0.0f` = far, keep larger z), which **resolves the model's front and back surfaces correctly** — the lesson's exit condition. Back-face culling is implemented as an optional flag driven by the sign of the signed area. Remaining: confirm the cull rate (the benchmark shows only ~10% saved where ~2× is expected), then unit tests for the three `screen::` functions and the depth test itself.

Per-lesson design docs live in [`docs/lessons/`](docs/lessons/).

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
| 3 | Hidden face removal (z-buffer) | In progress |
| 4 | Texture mapping | Planned |
| 5 | Lighting (Gouraud, Phong) | Planned |
| 6 | Camera & perspective | Planned |
| 7 | Shaders (moving to GPU) | Planned |

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

