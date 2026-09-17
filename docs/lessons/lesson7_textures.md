# Lesson 7 — More data! (textures, specular & glow maps)

**Source:** https://haqr.eu/tinyrenderer/textures/

> **Status: DONE (started 2026-09-11, doc written 2026-09-17).**
> `ObjLoader` reads `vt`; `UvColorShader` proved that parse on screen before anything relied on it;
> `Texture` + `io::loadTexture()` decode a TGA into owned RGBA; `TextureShader` renders flat
> diffuse; `MaterialShader` is the lit end-state — diffuse, specular colour and emission, all
> sampled per texel, over Blinn-Phong. Written after the fact, as Lesson 6's was: the spike ran
> first, across Sessions 52–56, and this document records the design those sessions earned.
>
> This is the first lesson built under the **rung policy** (`CLAUDE.md`) rather than the old
> naive-to-optimised rule. The lesson page walks diffuse → spec → glow as separate steps; they are
> three reads of one texture path, so they were folded into one shader.

## Goal

Stop describing the surface with uniforms and start reading it out of images — so that what a
point on diablo *is* (its albedo, where it shines, where it glows) is a property of the model's
texture set rather than a constant picked in the call site.

## Exit condition

- **UVs parse and interpolate.** `UvColorShader` writes `{u*255, v*255, 0}` and diablo comes up in
  continuous red/green gradients with the UV shell cuts visible. Per-triangle confetti would mean
  the `f v/vt/vn` triads are misaligned.
- **The diffuse map lands the right way up.** Deliberately left undecided so the failure would be
  seen rather than reasoned about — see *The v-flip* below.
- **The specular map controls where highlights appear**, and `specularIntensity_` scales the
  highlight linearly when swept 0 → 1 → 4.
- **The glow map reads as self-illumination** — visible in shadow, because it is added after
  lighting and never multiplied by `N·L`.
- `MaterialShader` A/B's against `BlinnPhongShader` and `TextureShader`, both of which stay in the
  tree per *keep superseded rungs*.

---

## Concepts

### UV coordinates and the third `vt` value

An OBJ `vt` line carries up to three floats. Every one of diablo's 3263 `vt` lines carries three,
and the third is **not** zero (~0.6 typical). It is the `w` of a 3D texture coordinate and nothing
in this renderer consumes it. `Mesh` stores the full `Vec3f` anyway so the mapping from file to
memory stays one-to-one; the drop to two components happens in each shader's `vertex()`, when it
fills its `Vec2f` varying. Measured before trusting any picture: diablo's u and v both span
**0 to 1**, so nothing relies on a wrap rule.

### Sampling: scale, truncate, clamp

`sample()` takes raw uv floats and does the whole conversion itself — scale by the dimensions,
truncate to an integer texel, clamp, index. Putting the scale inside `Texture` is what makes the
v-flip a texture-space concern rather than something every shader has to remember.

**The clamp goes on the integer, not the incoming float.** Diablo's UVs reach 1.0 *inclusive*, and
clamping the float to 1.0 still lets `1.0f * width_` produce exactly `width_` — one texel past the
end of the row. Clamping after the truncation is the only placement that closes it. (This is also
where `std::clamp` bites: it returns a `const&` and cannot modify its argument, so discarding the
return value makes the call a silent no-op.)

### The v-flip

`stbi_load` hands back the image **top-down**: index 0 is the top-left texel. The OBJ convention
puts `v = 0` at the **bottom** of the image. Nothing about the sampling arithmetic is wrong — the
two conventions simply disagree, and the correction is one line.

It was written whichever way read naturally and diagnosed on screen, upside down, which is what a
*show it* concept is for. The fix, `texture_y = (height_ - 1) - texture_y`, is placed **after** both
clamps: at that point the value is known to be in `[0, height_-1]`, so the flip is a bijection on a
valid range. Flipping first also works but means clamping a value that can legally be `-1`.

Aside: TGA on disk is usually stored bottom-up. stb normalizes it. The flip is a convention
mismatch, not a decoder quirk.

### Nearest sampling, and no filtering

The texel is chosen by truncation — nearest neighbour. No bilinear filtering, no mips. At diablo's
texture-to-screen ratio it is not the visible error, and filtering is not what this lesson teaches.

### What each map drives

| Map | Feeds | Gated by `N·L`? |
|---|---|---|
| `_diffuse.tga` | albedo — replaces `BlinnPhongShader`'s `baseColor_` per texel | yes (and tints the ambient term) |
| `_spec.tga` | specular **colour** — replaces `specularColor_` per texel | no, but gated by `(N·H)ᵉ` |
| `_glow.tga` | emission, added after lighting | **no** — that is what makes it read as self-illumination |

TinyRenderer feeds `_spec.tga` in as the per-texel **exponent** (`pow(..., spec_sample)`, raw byte,
0–255). That was rejected on inspection: the file reads as specular colour, and this project's
working convention separates specular intensity from a roughness map driving the exponent. Confirmed
on screen — the highlight is correct with the map as colour and the exponent as a free scalar.

### Where the intensity multiplier goes relative to `pow()`

`pow(N·H · k, e)` is `pow(N·H, e) · k^e`, not `pow(N·H, e) · k`. Written the first way it is exactly
correct at `k = 1` and catastrophically wrong anywhere else: at `e = 100`, `k = 0.5` scales the
highlight by ~8e-31 and the specular map appears to do nothing at all. The multiplier belongs
**outside** the exponentiation.

### UVs are still interpolated in screen space

Perspective-correct interpolation is deferred to the end of the project by decision (Session 52).
Every varying this lesson adds — and the world position the shadow pass will add — is blended with
screen-space barycentric weights until then. On diablo's small triangles the error is small. The
cost of deferring is a diagnostic one: **a slightly-off texture edge must not be misread as a
tangent-basis or shadow-bias bug in Lessons 8–9.**

---

## Design decisions

| Decision | Choice | Reason |
|----------|--------|--------|
| Texture data vs. loader | `Texture` in `src/rasterizer/`, `io::loadTexture()` in `src/io/TextureLoader.cpp` | Mirrors `Mesh` (`geometry/`) + `io::loadObj()`. The data type lives where it is consumed — shaders sample it per pixel — and the decoder stays in the io layer, so no stb header reaches the rasterizer. |
| Pixel ownership | copy stb's buffer into `std::vector<uint8_t>`, free stb's inside the loader | Keeps `Texture` a plain value type: no destructor, no copy/move rules, no stb in its header. Engines usually decode straight into their own allocation to skip the copy, but `stb_image` offers no hook for that. A GPU engine could drop the CPU copy after upload; a CPU rasterizer cannot — the pixels are sampled every frame. |
| Channel count | force RGBA via `desired_channels`, `static constexpr int CHANNELS = 4` on the class | Fixes the stride at `(y*w + x)*4`, so `sample()` constructs a `Color` (itself `static_assert`'d at 4 bytes) trivially. Costs ~33% memory over the files' likely 3. |
| `CHANNELS` a class constant, not a ctor parameter | class constant | Rejected argument: "the loader decides the count, so `Texture` needn't hold it." Counter: `sample()` indexes with the stride, so `Texture` must know it regardless — otherwise there are two unrelated `4`s in two files. As a parameter it would also open a hole `sample()` cannot honour: `channels = 3` makes it read the next texel's red as alpha. Configurable without working. |
| `Texture` is a `class` | class | `Mesh` is a struct because its fields *are* its interface. `Texture` has an invariant (`pixels_.size() == width_ * height_ * CHANNELS`) and behaviour (`sample()`), so its fields are an implementation detail behind one. |
| No default constructor | none | Two-phase construction would make "empty" a legal state of every `Texture`, so `sample()` would have to be correct on a 0×0 texture or be UB — reintroducing exactly the state the invariant forbids. It also fails mechanically: handing `stbi_load` the addresses of `width_`/`height_` needs them settable, and a setter for `width_` alone *is* the corruption. It saves nothing either, since the pixel copy is unavoidable. |
| Invariant enforced at construction | `assert` in the ctor body | The class blocks corruption *after* construction (no setters) but the ctor would otherwise believe any vector and any two ints. Explicitly **not** the input validation the rung policy drops — that rule covers known-good *data*; this is a programmer-error check. |
| Loader input validation | dropped | The models are known-good and the renderer is not used outside its boundaries. The one check kept is the returned pointer, which covers missing, unreadable, corrupt and unsupported-format in one test. |
| `channels_in_file` out-param | `nullptr` | Unused, since `Texture::CHANNELS` decides the layout. Verified rather than assumed: the vendored stb guards it on the TGA path (`if (comp) *comp = tga_comp;`). **stb is not uniformly null-safe** — the GIF and PNM paths write `*comp` unguarded. Safe for this project's five TGAs; not a habit to generalize. |
| Clamp vs. wrap | clamp | Diablo's UVs are in `[0,1]`, so nothing needs wrapping yet. |
| One shader or three | one `MaterialShader` | Diffuse, specular and glow are three reads of the same texture path. Under the rung policy they fold into one shader rather than three rungs. |
| Its name | `MaterialShader`, not `TexturedBlinnPhongShader` | A tangent-space normal map is a fourth read in this same class; shadow mapping adds a shadow-buffer lookup to it; AO follows. The lighting model stops being the interesting half of the name once the class samples a normal map and a shadow buffer — so it is named for the material, not the model. |
| Texture ownership in the shader | non-owning `const Texture*` | Same lifetime story as the `const Mesh*` it already holds: the caller keeps them alive. |
| Emission intensity | a scalar parameter | Without it the texture alone decides how hot the glow reads, and re-authoring the file is the only way to dial it. |
| Superseded shaders | kept | Per *keep superseded rungs*: `UvColorShader` stays as the UV-debug view, `TextureShader` as the flat-textured A/B, `BlinnPhongShader` as the untextured one. |
| Global-space `_nm.tga` | a reference render, not a class | It is the answer key the Lesson 8 tangent-space version must match, and only valid while the model matrix is identity. A one-off check, not a standing rung. |

---

## Modules

### `geometry/Mesh.h` (extended)
**Added:** `std::vector<tinymath::Vec3f> textureCoordinates` and
`std::vector<std::array<int,3>> faceTextureCoordinateIndices`.

### `io/ObjLoader.cpp` (extended)
Parses `vt ` and reads all nine indices from an `f v/vt/vn` face, with the 1-based → 0-based offset
applied in one loop.

### `rasterizer/Texture.h/.cpp`
**Responsibility:** owns a decoded RGBA image and converts a uv into a `Color`.
**API:**
- `Texture(std::vector<uint8_t> pixels, int width, int height)` — sink parameter, `std::move`d into
  the member; asserts the size invariant.
- `Color sample(float u, float v) const` — scale → truncate → clamp → v-flip → index.
- `static constexpr int CHANNELS = 4`

### `io/TextureLoader.h/.cpp`
**Responsibility:** decode an image file into a `Texture`.
**API:**
- `Texture io::loadTexture(std::string const& path)` — `stbi_load` with `desired_channels = 4`,
  throws `std::runtime_error` on a null return, frees stb's buffer before returning.

Carries the project's only `#define STB_IMAGE_IMPLEMENTATION`.

### `rasterizer/shaders/UvColorShader.h/.cpp`
**Responsibility:** render interpolated uv as colour. The UV-parse verification, kept as a debug view.

### `rasterizer/shaders/TextureShader.h/.cpp`
**Responsibility:** unlit diffuse texturing — the flat-textured A/B.

### `rasterizer/shaders/MaterialShader.h/.cpp`
**Responsibility:** the lit end-state shader for the rest of the series. Blinn-Phong with albedo,
specular colour and emission read per texel.
**API:**
- ctor `(mesh, diffuse_texture, specular_texture, emission_texture, transform, light_direction,
  view_direction, diffuse_intensity, specular_intensity, shininess, emission_intensity,
  ambient_intensity)` — normalizes both direction vectors in the init list.
- `vertex()` stashes both `varyingNormals_` and `varyingUvs_`.
- `fragment()` blends both, samples all three maps at the blended uv, and composites
  `diffuse·N·L + specular·(N·H)ᵉ + emission + diffuse·ambient`, saturated at 255.

### `tests/test_texture.cpp`
**Cases:** all against hand-built pixel vectors — no file on disk, since the assets are gitignored
and the loader is verified by the throw and by the picture.
- `v = 0` samples the bottom of the image, not the first row in memory (the v-flip)
- the four bytes come back as `r, g, b, a` in that order (the `pixels_[index] + 3` bug)
- `u` and `v` of exactly 1.0 stay inside the image (the inclusive-1.0 clamp)
- a texel owns the half-open range that truncates into it (nearest sampling, `[0.0, 0.5)` on a
  2-wide texture)
- a non-square texture indexes rows by width, not height — a 2×2 fixture cannot catch a transposed
  index, because `(y*w + x)` and `(x*h + y)` agree when `w == h`
- a 1×1 texture returns its only texel for every uv

The constructor `assert` is deliberately untested: Catch2 cannot catch an `assert`, it aborts.
Testing it would mean changing it to a throw, which is a design decision, not a test.

---

## Traps hit

- **`#define STD_IMAGE_IMPLEMENTATION`** (typo). The header still compiles — all declarations are
  present — and the build dies at *link* time on an unresolved `stbi_load`, which reads like a CMake
  wiring problem.
- **An `ifstream` in front of `stbi_load`**, copied from `ObjLoader`. stb opens, reads, decodes and
  closes the file itself; the guard opened the same file twice and checked strictly less.
- **The byte count written as `width * height * channels`.** `channels` reports what was *in the
  file*, not what stb returned — a 3-channel TGA would copy three quarters of the buffer and
  `sample()` would read past the vector for the rest. Must be `Texture::CHANNELS`.
- **`std::clamp`'s return value discarded**, twice — the clamp was a no-op and the exact
  out-of-range case it was added for still read past the row.
- **`pixels_[index] + 3`** instead of `pixels_[index + 3]` — blue plus three, not alpha.
- **Sink parameter taken by value but not moved.** Decorative unless the init list actually moves;
  the same hole existed at `return Texture(pixels, ...)`, since implicit move applies to *returning*
  a local, not to passing one as an argument.
- **`specularIntensity_` inside `pow()`** — invisible at the call site's `k = 1.0f`. See above.
- **Direction vectors not normalized in the init list**, unlike `BlinnPhongShader` — masked by a
  call site that happened to pass unit vectors. `normalize(L + V)` is only the true half vector when
  both are unit.
- **Assets are CWD-relative.** The exe runs from `build/`, and the five `.tga` files live at
  `models/obj/diablo3_pose/`. They must be hand-copied into `build/models/` on every clean build —
  **no CMake copy rule exists**. Without it `stbi_load` returns null and it looks like a code bug.

## Open at the close

- **No CMake asset-copy rule.** The resolver, when written, must not be named `FileUtils.h`.
- **Texture dimensions and the files' true channel count are still unmeasured** — `channels_in_file`
  is `nullptr`, so it is unknown whether the sampled alpha is file data or stb's synthesized 255.
  Matters the moment a shader branches on alpha. One run with the address taken back answers it.
- **Perspective-correct interpolation**, deferred to the end of the project.
