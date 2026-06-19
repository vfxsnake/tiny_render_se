# Tiny Renderer SE — Implementation Plan

Source: https://haqr.eu/tinyrenderer/

---

## Phase 0 — Infrastructure (prerequisite for all lessons)

Goal: get a window open that can display a CPU-rendered pixel buffer.

### 0.1 — First build
- Verify CMakeLists.txt builds cleanly on Windows (MSVC or MinGW)
- Confirm GLFW window opens and closes without errors
- `src/main.cpp` and `src/Application.h/.cpp` are the only files

### 0.2 — Framebuffer
- `src/rasterizer/Framebuffer.h/.cpp`
- Owns a `uint8_t` RGBA pixel buffer and a `float` z-buffer
- API: `clear()`, `setPixel(x, y, color)`, `setDepth(x, y, depth)`, `getDepth(x, y)`, `data()`
- Width and height fixed at construction
- Tests: `tests/test_framebuffer.cpp` — verify clear, setPixel, z-buffer read/write

### 0.3 — Timer utility
- `src/utils/Timer.h` — header-only, wraps `std::chrono::high_resolution_clock`
- API: `start()`, `stop()`, `elapsedMs()`, `elapsedUs()`
- Used to benchmark rasterizer iterations (e.g. naive vs. optimised line drawing, triangle fill strategies)
- No tests needed — correctness is verified by plausible output values

### 0.4 — Display pipeline
- `src/display/DisplayPipeline.h/.cpp`
- Each frame: receives a `const uint8_t*` RGBA buffer → uploads to a GPU staging buffer → copies to a texture → renders a fullscreen quad sampling that texture
- No vertex buffers, no UBO, no depth attachment
- Shaders: `shaders/display.slang` — passthrough vert (clip-space quad), frag samples texture
- Uses engine modules: `VulkanContext`, `SwapChain`, `FileUtils`
- Verified visually only (no unit tests)

### 0.5 — Debug buffer toggle
- `Application` owns a `ViewMode` enum: `Color`, `Depth` (more added as lessons require — e.g. `Normals` in Lesson 9)
- GLFW key callback cycles through available modes on a key press (e.g. `Tab`)
- Each frame, `Application` selects which buffer to pass to `DisplayPipeline`:
  - `Color` → `Framebuffer::data()` (the RGBA pixel buffer, passed directly)
  - `Depth` → `Framebuffer::depthAsRgba()` (converts float z-values to greyscale RGBA, returned as a temporary buffer)
- Current mode and timer results print to console — no on-screen overlay
- No ImGui for now; revisit if a lesson (e.g. Lesson 7 shading) benefits from live parameter tweaking

---

## Lesson 1 — Line Drawing

Reference: https://haqr.eu/tinyrenderer/lesson1

### Math
- `src/math/Vec2.h` — `Vec2<T>`, supports `float` and `int`
- Free functions: none needed yet
- Tests: `tests/test_vec2.cpp`

### Rasterizer
- `src/rasterizer/LineDrawer.h/.cpp`
- Implements Bresenham's line algorithm
- API: `drawLine(x0, y0, x1, y1, color, framebuffer)`
- Tests: `tests/test_line_drawer.cpp` — draw lines at 0°, 45°, 90°, 135°, assert pixel positions

### Visual milestone
Wireframe render of `models/african_head.obj` in the Vulkan window.

---

## Lesson 2 & 3 — Triangle Rasterization + Barycentric Coordinates

Reference: https://haqr.eu/tinyrenderer/lesson2  
Reference: https://haqr.eu/tinyrenderer/lesson3

### Math
- `src/math/Vec3.h` — `Vec3<T>`, supports `float` and `int`
- Free functions: `dot()`, `cross()`
- Tests: `tests/test_vec3.cpp`

### Rasterizer
- `src/rasterizer/TriangleRasterizer.h/.cpp`
- Bounding box + barycentric coordinate test per pixel
- API: `drawTriangle(v0, v1, v2, color, framebuffer)`
- Tests: `tests/test_triangle_rasterizer.cpp` — filled triangle, edge pixels, degenerate cases

### Visual milestone
Filled flat-colored triangles covering the head model.

---

## Lesson 4 — Hidden Face Removal (Z-Buffer)

Reference: https://haqr.eu/tinyrenderer/lesson4

### Rasterizer
- Extend `TriangleRasterizer` to interpolate depth and write/test z-buffer via `Framebuffer`
- Add backface culling: discard triangles whose normal faces away from camera
- OBJ loading: `src/rasterizer/ObjLoader.h/.cpp` — parse vertices, faces, normals, UVs
- Tests: `tests/test_zbuffer.cpp` — two overlapping triangles, verify front one wins

### Visual milestone
Solid shaded head with correct depth ordering, no backfaces.

---

## Lesson 5 & 6 — Camera & Projection

Reference: https://haqr.eu/tinyrenderer/lesson5  
Reference: https://haqr.eu/tinyrenderer/lesson6

### Math
- `src/math/Vec4.h` — `Vec4<T>`
- `src/math/Matrix4x4.h` — 4×4 float, row-major
- Operations: `operator*` (mat×mat, mat×vec4), transpose, inverse
- Free functions: `normalize()`, `length()`
- Tests: `tests/test_matrix4x4.cpp` — identity, multiply, inverse round-trip

### Rasterizer
- `src/rasterizer/Camera.h` — view matrix, perspective projection, viewport transform
- Tests: `tests/test_camera.cpp` — known point projections

### Visual milestone
Head rendered from an arbitrary camera position with perspective.

---

## Lesson 7 — Shading

Reference: https://haqr.eu/tinyrenderer/lesson7

### Rasterizer
- Flat shading: one normal per face, constant color scaled by `dot(normal, light_dir)`
- Gouraud shading: interpolate per-vertex normals across triangle using barycentric coords
- `src/rasterizer/Shader.h` — interface: `vertex()` transform + `fragment()` color
- Tests: `tests/test_shading.cpp` — verify diffuse intensity calculation

### Visual milestone
Smooth-shaded lit head.

---

## Lesson 8 — Texture Mapping

Reference: https://haqr.eu/tinyrenderer/lesson8

### Rasterizer
- `src/rasterizer/Texture.h/.cpp` — load via `stb_image`, sample by UV
- Extend triangle rasterizer to interpolate UVs and sample texture in fragment step
- Tests: `tests/test_texture.cpp` — load a 2×2 test image, verify sample at corners

### Visual milestone
Textured head (`models/african_head_diffuse.tga`).

---

## Lesson 9 — Normal Mapping (Tangent Space)

Reference: https://haqr.eu/tinyrenderer/lesson9

### Rasterizer
- Extend `Texture` to load normal maps
- Compute tangent-space basis (tangent, bitangent, normal) per triangle
- Transform sampled normal into world space for lighting
- Tests: `tests/test_normal_mapping.cpp` — verify TBN matrix construction

### Visual milestone
Normal-mapped head with surface detail.

---

## Lesson 10 — Shadow Mapping

Reference: https://haqr.eu/tinyrenderer/lesson10

### Rasterizer
- Two-pass render: first from light POV → shadow buffer; second from camera POV → sample shadow buffer
- `src/rasterizer/ShadowMap.h/.cpp`
- Tests: `tests/test_shadow_map.cpp` — occluded point reads correct shadow depth

### Visual milestone
Head with cast shadows.

---

## Lesson 11 — Ambient Occlusion

Reference: https://haqr.eu/tinyrenderer/lesson11

### Rasterizer
- Screen-space AO pass over the completed framebuffer
- `src/rasterizer/AmbientOcclusion.h/.cpp`
- Tests: `tests/test_ambient_occlusion.cpp`

### Visual milestone
Head with contact shadowing and soft occlusion.

---

## Lesson 12 (Bonus) — Toon Shading

Reference: https://haqr.eu/tinyrenderer/lesson12

### Rasterizer
- Quantize diffuse intensity to discrete steps
- Silhouette edge detection pass

### Visual milestone
Cel-shaded head.

---

## Phase 2 — GPU Compute Port (future)

After all CPU lessons are complete, port the rasterizer to compute shaders using the CPU implementation as the reference. The `Framebuffer` boundary makes this a clean swap — the display pipeline does not change.

---

## Dependency Graph

```
Phase 0 (Framebuffer + Display)
    └── Lesson 1 (Vec2, LineDrawer)
            └── Lesson 2/3 (Vec3, TriangleRasterizer)
                    └── Lesson 4 (Z-Buffer, ObjLoader)
                            └── Lesson 5/6 (Vec4, Matrix4x4, Camera)
                                    └── Lesson 7 (Shading)
                                            └── Lesson 8 (Texture)
                                                    └── Lesson 9 (Normal Map)
                                                            └── Lesson 10 (Shadow Map)
                                                                    └── Lesson 11 (AO)
                                                                            └── Lesson 12 (Toon)
```
