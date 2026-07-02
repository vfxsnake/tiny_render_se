# Phase 0.2 — Framebuffer

**Source:** Project implementation plan (`docs/implementation_plan.md`, §0.2). Infrastructure prerequisite — not a TinyRenderer lesson, but follows the same design-first protocol.

## Goal
Provide the shared pixel/depth buffer that is the boundary of the whole system: the rasterizer writes pixels into it, the display pipeline reads pixels out of it, and neither side knows the other's internals.

## Exit condition
- `Framebuffer.h/.cpp` compiles under both toolchains (WSL/Ninja and Windows/MSVC).
- Catch2 is wired in via FetchContent.
- `tests/test_framebuffer.cpp` passes: verifies `clear()`, `setPixel()` (in-bounds writes + out-of-bounds ignored), and z-buffer read/write.
- No window/visual check yet — the buffer becomes visible output in Phase 0.4 (display pipeline).

## Concepts

### Storage vs. computation
The buffer stores **8-bit** RGBA (LDR) because that is what the GPU display texture and image files use. Color *math* (e.g. lighting in Lesson 7) is done in floats `[0, 1]` and converted to 8-bit only at the moment of writing. HDR (float storage, values > 1.0) is deliberately out of scope — TinyRenderer's lighting never leaves `[0, 1]`, so float storage would be premature abstraction.

### Memory layout & origin
Row-major array of `Color` values: `index = y * width + x` (a *pixel* slot, not a byte offset). The buffer is stored as `std::vector<Color>` so the container carries the pixel size — no manual stride, no magic `4`. Because `Color` is exactly 4 tightly-packed bytes, the vector's memory *is* a contiguous interleaved RGBA byte stream, so `getData()` hands the display pipeline a `reinterpret_cast<const uint8_t*>(pixels_.data())` with zero copy or repacking. The framebuffer is a *dumb* 2D array with `y = 0` at the **top row**. The math y-up vs. texture y-down flip is handled later in the **display pipeline**, not here — keeps the boundary simple.

The `reinterpret_cast` is only sound if `Color` has no padding, so `Color.h` carries a `static_assert(sizeof(Color) == 4)` to make that contract a compile-time guarantee.

### Color is not a vector
`Color` is data with a fixed format (8-bit storage), not a math primitive. It lives in `rasterizer/`, never in `math/`, and is never merged with `Vec4`. A `Vec4` supports `dot`/`cross`/`normalize` (meaningless on color); a `Color` is bounded `[0, 255]`. Keeping them distinct lets the compiler catch "passed a normal where a color was expected" bugs. When Lesson 7 needs float→8-bit conversion, we **add** a `Color::fromFloat` factory — we do not change `Color`'s storage.

### Bounds checking = stand-in for clipping
Un-clipped rasterization legitimately generates off-screen coordinates: lines/triangles that extend past the viewport (Bresenham walks past the edge), bounding boxes that poke out, edge rounding (`x == width`). TinyRenderer does not clip in early lessons. So `setPixel` guards its own memory: one `if` at the boundary protects every current and future caller, instead of scattering bounds checks through every algorithm. Off-screen write → silently do nothing.

### Z-buffer convention
One `float` per pixel, separate buffer, used from Lesson 4. The exact clear value (TinyRenderer keeps the largest z, clears to `-infinity`) is deferred to Lesson 4; for now `clear()` resets depth to a sentinel and the convention is finalized when hidden-surface removal is implemented.

## Design decisions
| Decision | Choice | Reason |
|----------|--------|--------|
| Color storage | `struct Color { uint8_t r, g, b, a; }` in `rasterizer/` | 8-bit is the GPU texture / file format; LDR covers every lesson |
| Buffer container | `std::vector<Color>` (not `std::vector<uint8_t>`) | Container carries the pixel size — kills the manual stride, the magic `4`, and the Framebuffer→Color size coupling. `index()` = `y*width+x` indexes it directly. |
| GPU boundary | `getData()` = `reinterpret_cast<const uint8_t*>(pixels_.data())` | `Color` is packed 4-byte RGBA, so the vector's bytes already are the interleaved stream Vulkan wants — zero-copy. Guarded by `static_assert(sizeof(Color) == 4)` in `Color.h`. |
| Float color helper | Deferred (no `fromFloat` yet) | No consumer until Lesson 7 shading — avoid premature abstraction |
| Color vs. Vec4 | Keep separate, never merge | Different meaning, range, and storage; type-safety catches bugs |
| Memory layout | Row-major array of `Color`, `index = y*width+x` | Matches GPU upload + image file layout; contiguous packed RGBA |
| Origin / y-flip | `y=0` = top row; flip handled in display pipeline | Framebuffer stays a dumb array; simple boundary |
| Bounds checking | Guard inside `setPixel`, silently ignore out-of-bounds | Single safety net for all callers; stand-in for clipping |
| Depth type | `float` z-buffer, separate from color | Standard; exact clear value finalized in Lesson 4 |
| Dimensions | Fixed at construction (`width_`, `height_`) | Resize not needed for the lesson series |

## Modules

> **Style note:** API signatures use **west const** (`const Color&`) and **`get`-prefixed getters** (`getWidth`, `getData`), matching the engine convention (`SwapChain::getFormat()`, etc.) and CLAUDE.md.

### `src/rasterizer/Color.h`
**Responsibility:** Plain 8-bit RGBA color value type. Header-only.
**API:**
- `struct Color { uint8_t r; uint8_t g; uint8_t b; uint8_t a; }` — aggregate, no methods yet
- `static_assert(sizeof(Color) == 4, ...)` — guarantees tight packing for the `reinterpret_cast` GPU upload path
- (Future, Lesson 7) `static Color fromFloat(float r, float g, float b, float a)` — clamp + scale by 255

### `src/rasterizer/Framebuffer.h/.cpp`
**Responsibility:** Owns the RGBA color buffer and the float z-buffer. The system boundary.
**API:**
- `Framebuffer(int width, int height)` — allocates both buffers; dimensions fixed
- `void clear(const Color& color)` — fill color buffer with `color`; reset z-buffer to its sentinel
- `void setPixel(int x, int y, const Color& color)` — bounds-guarded write into the color buffer (off-screen → no-op)
- `void setDepth(int x, int y, float depth)` — bounds-guarded write into the z-buffer
- `float getDepth(int x, int y) const` — read z-buffer (out-of-bounds read returns the sentinel)
- `const uint8_t* getData() const` — `reinterpret_cast` of the `Color` buffer to a raw RGBA byte pointer for the display pipeline
- `int getWidth() const` / `int getHeight() const` — accessors
- (Future, Phase 0.5) `depthAsRgba()` — greyscale visualization of the z-buffer for the debug view toggle

**Member layout:**
- `int width_`, `int height_`
- `std::vector<Color> pixels_` — size `width_ * height_` (one `Color` per pixel; no stride member)
- `std::vector<float> depth_` — size `width_ * height_`

### `tests/test_framebuffer.cpp`
**Cases:**
- Construct a small framebuffer (e.g. 4×4); `width()`/`height()` report correct dimensions.
- `clear(color)` sets every pixel to that color (`data()` spot-checks).
- `setPixel` writes the expected 4 bytes at `(y*width+x)*4`.
- `setPixel` with out-of-bounds coords (negative and ≥ dimension) is a no-op — surrounding pixels unchanged, no crash.
- `setDepth`/`getDepth` round-trip a known value at a known pixel.
- `getDepth` out-of-bounds returns the sentinel (no crash).

### `tests/CMakeLists.txt` + Catch2
- Add Catch2 v3 via FetchContent (first test in the project).
- Register `test_framebuffer` as a CTest target.
