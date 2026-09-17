# Tiny Renderer SE

A software rasterizer built from scratch, following the [TinyRenderer](https://haqr.eu/tinyrenderer/) lecture series, displayed live inside a Vulkan window provided by the [Snake Engine](https://github.com/vfxsnake/vk_tutorial_se).

This is a learning project with two goals:

1. Implement every classic rasterization algorithm by hand — lines, triangles, z-buffering, texture mapping, lighting — entirely on the CPU, no graphics API shortcuts.
2. Bridge that CPU output into a live interactive window so the result can be seen in real time, not just written to a file.

Porting these algorithms to GPU compute shaders is **a separate project**, not a phase of this one — parallel rasterization is its own body of knowledge (SIMT execution, tiling/binning, depth-buffer atomics) and only incidentally about Vulkan. This project's CPU rasterizer is the reference implementation it will be checked against.

---

## Current status

**As of 2026-09-16 — Lesson 7 (More data!) in progress; Lessons 0-6 complete.**

- **Phase 0 - Display pipeline: complete.** Vulkan window showing a CPU framebuffer uploaded each frame through a staging buffer and sampled over a fullscreen quad. Renderer-agnostic, so it seeds a future ray-tracing project unchanged.
- **Lessons 1-5: complete.** Bresenham lines, barycentric triangle rasterization, z-buffer, perspective projection, and camera (`lookAt` + viewport matrix). All math hand-written in `src/math/` - no GLM anywhere in the rasterizer.
- **Lesson 6 - Shading: complete.** `AbstractShader` fixes the two-stage contract (`vertex()` per corner, `fragment()` per candidate pixel with barycentric weights, `false` discards). Random, Face, Gouraud, Lambert, Phong and Blinn-Phong all remain in the tree as standing A/Bs rather than being replaced.
- **Lesson 7 - in progress.** The OBJ loader reads `vt`, and `UvColorShader` confirmed that parse on screen before anything relied on it. `Texture` (owns its RGBA pixels, `sample()` scales the uv and clamps on the final integer index) and `io::loadTexture()` on stb_image are written and working: **diablo now renders with its diffuse map.** The v-flip was left undecided on purpose and diagnosed the intended way - on screen - then fixed inside `sample()`, since row 0 is the top of the image while `v = 0` is the bottom.
- **Next: `MaterialShader`** - Blinn-Phong with the diffuse map as base colour, the specular map as specular colour, and the glow map as emissive, leaving the exponent and ambient as free parameters. Normal mapping, shadow mapping and ambient occlusion all grow into this same class, which is why it is not named after its lighting model.

Known gaps: the five `.tga` maps must be hand-copied into `build/models/` on every clean build (no CMake copy rule yet), and perspective-correct interpolation is deliberately deferred to the end of the series.

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
| 6 | Shading | Complete |
| 7 | More data! (textures, normal & specular maps) | In progress |
| 8 | Tangent space | Planned |
| 9 | Shadow mapping | Planned |
| 10 | Ambient occlusion | Planned |
| 11 | Toon shading (bonus) | Planned |
| — | GPU compute port | Split into a separate project |

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

