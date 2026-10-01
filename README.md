# Tiny Renderer SE

A software rasterizer built from scratch, following the [TinyRenderer](https://haqr.eu/tinyrenderer/) lecture series, displayed live inside a Vulkan window provided by the [Snake Engine](https://github.com/vfxsnake/vk_tutorial_se).

This is a learning project with two goals:

1. Implement every classic rasterization algorithm by hand — lines, triangles, z-buffering, texture mapping, lighting — entirely on the CPU, no graphics API shortcuts.
2. Bridge that CPU output into a live interactive window so the result can be seen in real time, not just written to a file.

Porting these algorithms to GPU compute shaders is **a separate project**, not a phase of this one — parallel rasterization is its own body of knowledge (SIMT execution, tiling/binning, depth-buffer atomics) and only incidentally about Vulkan. This project's CPU rasterizer is the reference implementation it will be checked against.

---

## Current status

**As of 2026-10-01 — Lessons 0-10 complete. Lesson 11 (Toon shading) in progress: banded shading done, outline pass next.**

- **Phase 0 - Display pipeline: complete.** Vulkan window showing a CPU framebuffer uploaded each frame through a staging buffer and sampled over a fullscreen quad. Renderer-agnostic, so it seeds a future ray-tracing project unchanged.
- **Lessons 1-5: complete.** Bresenham lines, barycentric triangle rasterization, z-buffer, perspective projection, and camera (`lookAt` + viewport matrix). All math hand-written in `src/math/` - no GLM anywhere in the rasterizer.
- **Lesson 6 - Shading: complete.** `AbstractShader` fixes the two-stage contract (`vertex()` per corner, `fragment()` per candidate pixel with barycentric weights, `false` discards). Random, Face, Gouraud, Lambert, Phong and Blinn-Phong all remain in the tree as standing A/Bs rather than being replaced.
- **Lesson 7 - More data!: complete.** The OBJ loader reads `vt`, and `UvColorShader` confirmed that parse on screen before anything relied on it. `Texture` owns its decoded RGBA pixels; `sample()` scales the uv, truncates, clamps on the final integer index and flips v - row 0 is the top of the image while `v = 0` is the bottom, a mismatch left undecided on purpose and diagnosed the intended way, on screen. `MaterialShader` is the lit end-state: Blinn-Phong reading albedo, specular **colour** and emission from three maps per texel, with the exponent, ambient and per-component intensities as free parameters. The lesson page feeds the spec map in as a per-texel exponent; it reads as colour and is used as colour here. `TextureShader` (flat diffuse) and `BlinnPhongShader` (untextured) stay in the tree as A/Bs. `tests/test_texture.cpp` passes.
- **Lesson 8 - Tangent space: complete.** A fourth texture read in `MaterialShader`. Per pixel, the triangle's tangent and bitangent are solved from its position edges and UV deltas (`[t b] = E · U⁻¹`, written out as a 2×2 solve), and the decoded `_nm_tangent.tga` texel is mapped through `t·x + b·y + n·z` with the interpolated normal as `n`. The determinant's sign is kept, so mirrored UV islands (limbs, tail) flip correctly. Feeding the tangent map in as a global-space map was run deliberately first: every normal sits near `(0,0,1)`, so the model lights flat. Verified by eye; no zero-area-UV guard, no orthogonalization, no unit tests.
- **Lesson 9 - Shadow mapping: complete.** The first *global* term in the renderer: Blinn-Phong cannot know another triangle exists, so visibility is answered by rendering the scene twice. A depth-only `DepthShader` pass from a directional light (normalized `{1,1,1}`, `lookAt` with no `perspective()` - that is already the orthographic light camera) fills a second, CPU-only `Framebuffer`; `MaterialShader` then interpolates the object-space position varying Lesson 8 already added, maps it through `viewport(shadow map size) x light_matrix`, and compares its depth against the stored one, scaling diffuse and specular only so ambient and emission survive the shadow. That varying is why the lesson's `M-inverse` and a 4x4 matrix inverse were never needed. Measuring light-space bounds first showed `lookAt` ignores eye distance (it translates by the target, not the eye), so the model is fitted into the light frustum by a uniform 0.8 scale - without it, geometry at light-space z below -1 maps to a negative depth, loses against the cleared 0.0f and vanishes silently. Both failure modes were run on purpose: an inverted comparison swaps lit and shadowed, and zero bias produces acne - whose *vertical stripes* identified the cause as float-to-int truncation in the pixel lookup rather than floating-point noise, fixed by rounding, with `0.05f` of bias clearing the rest. Shadow mapping and ambient occlusion grow into `MaterialShader`, which is why it is not named after its lighting model.
- **Lesson 10 - Ambient occlusion: complete.** SSAO only - the brute-force many-shadow-maps bake is skipped. Samples live in object space in a hemisphere oriented by the normal-mapped normal, and are tested against a camera depth pre-pass using the same project-divide-round lookup as the shadow map, computed inside `MaterialShader::fragment()` via a dedicated `AmbientOcclusion` function. No normal buffer, position buffer or matrix inverse is needed. AO scales the ambient term only; the AO-only view is `MaterialShader` with every other term off and a 1x1 white diffuse texture. `AmbientOcclusion::estimateAmbientOcclusion` is implemented (flip into the hemisphere, offset by radius, project-divide-round, count samples whose stored depth is not nearer than the sample's depth plus bias). `MaterialShader` takes the camera depth buffer, AO lookup transform, sample count, radius and bias, and builds its sample vectors once with a fixed-seed rejection sampler inside the unit sphere - lengths kept, so samples fill the volume rather than a shell. Settled on screen at radius 0.2 (object units), 256 samples, bias 0: crevices and normal-map detail darken, flat areas stay clean, with slight speckle left in the deepest folds. The bias proved very sensitive - the viewport halves z, so a 0.2 radius reaches only about 0.1 in depth, and a 0.05 bias already erased everything but the deepest cavities. No range check or blur was needed; timing across sample counts was not measured.
- **Lesson 11 - Toon shading: in progress.** `ToonShader` is done and on screen: diffuse from the normal-mapped normal, quantized into bands chosen by the caller (`band_values`, e.g. `{0.2, 0.8, 1.0}`) rather than evenly spaced k/n, so shadow tones can go deeper than 1/n. Band index = `min(floor(d*n), n-1)`. The shadow map is folded into the diffuse term before quantizing, so shadowed pixels fall into the darkest band; a flat white specular spot appears where the Blinn-Phong highlight exceeds 0.5 (shininess 100). Next: the project's first post-process pass, a Sobel operator over the framebuffer's depth that paints outlines where depth jumps.

Known gaps: the five `.tga` maps must be hand-copied into `build/models/` on every clean build (no CMake copy rule yet), and perspective-correct interpolation was dropped by choice (varyings use screen-space barycentrics, so textures distort slightly on surfaces angled away from the camera).

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
| 7 | More data! (textures, normal & specular maps) | Complete |
| 8 | Tangent space | Complete |
| 9 | Shadow mapping | Complete |
| 10 | Ambient occlusion | Complete |
| 11 | Toon shading (bonus) | In progress |
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

