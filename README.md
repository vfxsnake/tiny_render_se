# Tiny Renderer SE

A software rasterizer built from scratch, following the [TinyRenderer](https://haqr.eu/tinyrenderer/) lecture series, displayed live inside a Vulkan window provided by the [Snake Engine](https://github.com/vfxsnake/vk_tutorial_se).

This is a learning project with two goals:

1. Implement every classic rasterization algorithm by hand — lines, triangles, z-buffering, texture mapping, lighting — entirely on the CPU, no graphics API shortcuts.
2. Bridge that CPU output into a live interactive window so the result can be seen in real time, not just written to a file.

A second phase (planned) will port the rasterizer algorithms to GPU compute shaders, using the CPU version as the reference implementation.

---

## Current status

**As of 2026-08-20 — Lesson 6 (Shading) is underway: flat world-space diffuse lighting is on screen. The lesson opened by reading the tree rather than the lesson page, and two facts there shape all of it: `io::Mesh` carries positions and face indices only — `ObjLoader` *actively skips* the `vn` index in every triad, so there are no normals in this program — and `Triangle` is three `RasterVertex` (`Vec2f` + `depth`), which **carries no attributes at all**, since world positions die at the perspective divide. Everything this lesson needs to interpolate has nowhere to live yet, and making room for it is the real structural change ahead. The first rung skips the loader entirely: `Application::testDrawMeshMatrixLightWorldSpace()` takes the three world-space positions it already fetches per face, computes `normalize(cross(b − a, c − a))`, dots it with a hardcoded light and scales a gray. The clamp is the known gap — a negative dot fed to `static_cast<uint8_t>` is undefined behaviour rather than wraparound, and back-face culling does **not** cover it, because facing the camera and facing the light are independent conditions. The run order was fixed deliberately: the *correct* world-space version first, the screen-space break second, since breaking first leaves no reference to judge against; and the light is hardcoded rather than random so the prediction is scorable. The shader-interface fork was taken *in* rather than deferred, against the "no premature abstraction" argument, which was put first and squarely. It lands as **`AbstractShader`** — two pure virtuals (`vertex()` / `fragment()`) and a virtual destructor, no shared state, deliberately mirroring the GLSL model. The `I` prefix was dropped as a COM/C# convention that would be the only Hungarian-flavoured name in the tree, and `ShaderBase` was rejected because `Base` promises shared machinery you inherit for free, which is only honest if state lives there. TinyRenderer's own uniform storage is explicitly *not* copied: the author uses file-scope globals in `our_gl.cpp`, and here the uniforms go on the concrete shader as members, closer to how a real GL program object works. One conceptual correction is worth recording, because it is the structural point of the lesson: a rasterizer does **not** go from a 2D pixel back to its 3D position to light it. Projection is destructive and not invertible per pixel — many 3D points collapse onto one, and the `w` divide throws the scale away — so the 3D data is **carried forward** as attributes the vertex stage attaches and the rasterizer blends with the barycentric weights it computed in 2D. That is what a varying is. A ray tracer genuinely has no such problem (the camera transform runs once to make a ray; the hit point is already a world position), and reconstructing world position from stored depth plus an inverse view-projection is a real technique — it is deferred shading, and it works only because depth was stored. The invariant underneath all of it: the space does not matter, so long as the normal, the light direction and the view direction are in the *same* one. Two carry-forwards from Lesson 5 were closed as decisions rather than as work. `docs/lessons/lesson4_camera_naive.md` is **struck for good** — the lesson closed on screen and was superseded by Lesson 5, so the doc would record history nobody reads. The `Framebuffer` depth convention is **parked with the framing sharpened**: the three bare `0.0f`s are not one thing (two are the *cleared depth* and must always agree; the third is `getDepth`'s out-of-bounds return, currently indistinguishable from a legitimately cleared pixel), and naming was rejected as the real fix — `0.0f` is only correct because the depth test reads `<`, keep-larger, an invariant spanning three files that no name on the literal can convey. If it lands it lands as a class-scoped `static constexpr float` on `Framebuffer`, not a member and not a free header constant. Lesson 5 (Better camera) is complete. The camera is now a position and a target the caller states, driven by a single matrix composed once per frame instead of three function calls per vertex. `math/Vec4.h` (templated aggregate + `toVec4`/`toVec3`, the latter being the perspective divide) and `math/Matrix4x4.h` (`float data[4][4]`, row-major storage, column-vector `M * v` convention, default-constructs to identity) are unit-tested and green, and `Vec3` has gained free `length()`/`normalize()` — the last entry on the original math-primitive list, forced into existence by `lookAt`. `normalize` now carries an `assert` against the zero vector, and that was decided by watching the failure rather than by argument: a camera looking straight down makes `cross(up, forward)` **exactly** zero, and the prediction (a crash) was wrong — IEEE 754 defines `0/0` as NaN instead of trapping, every comparison against NaN is false, so the rasterizer's bounding-box tests silently reject every pixel and the frame goes black with no diagnostic. That black frame is indistinguishable from the one a zero-fallback would produce, which is the recorded criterion the guard had to meet. `assert` rather than a branch, because a degenerate camera basis is a programming error, not runtime input. In `Projection.h`, `viewport(screen_width, screen_height)` and `perspective(focal_length)` are written — the viewport's depth row deliberately scales and translates `z` instead of passing it through as the lesson does, because our `0.0f` depth clear would otherwise swallow the entire back half of the model in silence. `lookAt(eye_position, target_position, up_vector)` now joins them in `Transform.h` — the orthonormal camera basis as **rows**, composed as `rotation * translate(−target)`. Note that it translates by `−target`, not `−eye`, which is not what a standard view matrix does: it is TinyRenderer's scheme, where the model lands centred on the camera-space origin and the eye's *distance* is carried entirely by the perspective matrix, so `focal_length` must equal `|eye − target|`. **The rows are correct, and that was settled on screen rather than by argument** — the transpose is this lesson's one silent bug, since it renders a plausible image at the same angle the wrong way. `Application::testDrawMeshMatrix()` composes `viewport * perspective * lookAt` once per frame and runs `toVec4` → multiply → `toVec3` per vertex. Two things had to happen in the right order to get a trustworthy answer. First a **regression check** against the spike, which does *not* settle the transpose question on its own — the camera position was chosen by matching the spike, so either convention could have been fitted to it — but does prove the composition order, the divide, the depth row and the viewport scale agree. It also could not be judged by eye: the model is a near bilaterally symmetric humanoid, so the two candidate camera positions render as mirror images and look identical, and the check had to be done in **numbers** instead, pushing a single point through both chains. That exposed a 0.1 px gap far too large to be float noise, traced by prediction to the spike's `0.785` not being π/4 — the matrix path was the *more* accurate of the two. Only then the transpose experiment, with an asymmetric camera at `eye = {1,1,3}`, where the discriminator is the **`y`, not the `x`**: the model is not symmetric top-to-bottom, so "above, looking down at the top of the head" is unambiguous and needs no reference render. Unit tests for the three builders were deliberately deferred until after that experiment, since writing one first would have required deciding the transpose question in advance and would then have locked the answer in whether or not it was right; they are now unblocked. The wrong version has now been rendered on purpose — basis into columns, and the image came back exactly as predicted: viewpoint swung below and mirrored, underside of the chin, no artifact anywhere to tip you off. For an orthonormal basis the transpose *is* the inverse, so the broken matrix is still a perfectly valid rotation, just the wrong way round. That finding is recorded where it is needed rather than only in the log: `Matrix4x4.h` now documents all four conventions the layout comment never captured — column vectors, right-to-left composition, translation in the fourth column, and orientation-as-rows with the transpose trap spelled out. A free `transpose()` was added for the experiment and kept (normal matrices will want it in Lesson 6), with three cases in `test_matrix4x4`: hand-written expected entries rather than a loop that restates the implementation, the double-transpose round trip, and `transpose(R) * R == identity` for a dense orthonormal `R` — the property the whole morning rested on. Compiler warnings are finally on (`/W4`, `-Wall -Wextra`, per target so the FetchContent dependencies stay quiet), which confirmed no missing `return` is hiding anywhere; `tests/CMakeLists.txt` collapsed seven copy-pasted blocks into one `add_project_test()` function so the flags are defined once. The three builders are now unit-tested too, in `test_transform` (8 cases) and `test_projection` (7 cases) — which also gives `Transform.cpp` and `Projection.cpp` their first compiler coverage outside MSVC. The discriminating case is pinned explicitly: with the camera on `+x`, a point at `+z` must come out at `x = −1`, and the columns version would give `+1`. The on-axis camera case is pinned too but annotated as proving *nothing* about the convention, since identity is its own transpose. Two pieces of debt are carried out of the lesson rather than closed with it: the `Framebuffer` depth convention (the bare `0.0f` clear at three call sites) is still undocumented at the point of use, and `docs/lessons/lesson4_camera_naive.md` remains the one lesson without a plan doc. The Lesson 4 spike functions (`rotateY` / `perspectiveZDivide` / `orthographicProjection`) are **kept, not retired** — the matrix path goes in as a second draw function alongside them, so the two pipelines can be rendered against each other. This is a learning project; the superseded rung stays as the reference the new one is checked against, the same way each line-drawing and triangle-rasterization rung was kept. See [`docs/lessons/lesson5_camera.md`](docs/lessons/lesson5_camera.md). Lesson 4 (Naive camera handling) is complete on screen — the model rotates about the y-axis under central projection via `rotateY` → `perspectiveZDivide` → `orthographicProjection`, still spike-quality (free functions, hardcoded 45° and eye distance 3) and the one lesson without a design doc. `Matrix3x3` was considered and **cancelled**: the 4×4 was needed anyway, and the hand-rolled divide collapses into it. Lesson 3 (Hidden face removal / z-buffer) is complete: 2.02× back-face culling on 5022 triangles, 44 unit tests.**

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

