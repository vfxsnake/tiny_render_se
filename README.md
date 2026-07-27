# Tiny Renderer SE

A software rasterizer built from scratch, following the [TinyRenderer](https://haqr.eu/tinyrenderer/) lecture series, displayed live inside a Vulkan window provided by the [Snake Engine](https://github.com/vfxsnake/vk_tutorial_se).

This is a learning project with two goals:

1. Implement every classic rasterization algorithm by hand — lines, triangles, z-buffering, texture mapping, lighting — entirely on the CPU, no graphics API shortcuts.
2. Bridge that CPU output into a live interactive window so the result can be seen in real time, not just written to a file.

A second phase (planned) will port the rasterizer algorithms to GPU compute shaders, using the CPU version as the reference implementation.

---

## Current status

**As of 2026-07-27 — Lesson 3 (Hidden face removal / z-buffer) in progress: math + asset-loading foundations done, rasterizer rewrite pending.**

Overall arc: work through the TinyRenderer lessons in order (CPU rasterizer, by hand) until an OBJ model head renders shaded in the live window, then a **Phase 2** ports the algorithms to GPU compute shaders using the CPU version as reference.

- **Phase 0 — Display pipeline: complete.** CPU framebuffer uploads to a Vulkan texture each frame and renders live in the window (fullscreen triangle, dynamic rendering). Tagged `phase0-display-complete` — this is the renderer-agnostic seed reused by a separate planned **Ray Tracing in One Weekend** project.
- **Lesson 1 — Line drawing: complete.** Naive → accumulator → integer-Bresenham ladder, benchmarked and unit-tested (differential test across all three rungs).
- **Lesson 2 — Triangle rasterization: complete.** Two rungs benchmarked (scanline vs bounding-box + barycentric); barycentric shipped as `drawTriangle` — slower on scalar CPU (~2.4–2.9×) but the pixel-independent, GPU-portable path. `Vec2` gained `cross`/`operator-`; unit-tested.
- **Lesson 3 — Hidden face removal (z-buffer): in progress.** Kick-off + design complete, plan doc written. Done so far: the `tinymath` namespace adopted across the math and rasterizer layers; `Vec3<T>` (+ `dot`/`cross`) written and unit-tested; a minimal OBJ loader (`io::loadObj` → vertices + face indices). Next: orthographic projection, a wireframe checkpoint, then the `drawTriangle` rewrite with barycentric depth interpolation and the per-pixel depth test (depth convention finalized: clear `0.0f` = far, keep larger z). Window also goes square (800×800).

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

# Run
./build/TinyRendererSE
```

Requires a Vulkan-capable GPU and the LunarG Vulkan SDK installed.

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

