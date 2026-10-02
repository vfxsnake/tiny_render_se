# Lesson 11 — Toon shading

**Source:** https://haqr.eu/tinyrenderer/toon/

> **Status: COMPLETE (Session 65, 2026-10-02).** Planned in Session 63 before any spike, at the
> user's request. Spike revised `band_count` → `band_values` and kept the shadow map (Session 64);
> outline pass moved to `postprocess/Outline` and its threshold measured on screen (Session 65).

## Goal

Replace smooth lighting with flat bands of brightness and draw black outlines where the depth
buffer jumps, so diablo reads as a cel-shaded drawing instead of a lit surface.

## Exit condition

1. **Bands.** The diffuse lighting on diablo shows a few flat steps with hard boundaries, no gradient.
2. **Outlines.** A black silhouette around the model plus inner lines at large depth breaks (horns,
   arms against the body), computed from the z-buffer after the shading pass.

---

## Scope

The lesson's two pieces only: quantized intensity and a Sobel outline pass. The homework's
extras (hatching textures, stylized colours, band placement tuning beyond the threshold sweep)
are **out** unless the user asks.

---

## Concepts

### Quantized intensity

Lighting normally gives a continuous value in [0, 1]. Toon shading snaps it to a few levels
before it scales the colour. The lesson uses three: `> 0.66 → 1.0`, `> 0.33 → 0.66`, else `0.33`.
The floor of 0.33 also acts as the ambient — no pixel facing away goes fully black.

### Edges from depth, not colour

An outline belongs where one surface ends and another, farther one begins. That is a jump in
**depth** between neighbouring pixels. Colour edges would also fire on texture detail and
shading bands; depth edges fire only on geometry.

### Sobel operator

Per pixel, weigh the 3×3 depth neighbourhood with two kernels: `Kx` measures the change left→right,
`Ky` top→bottom. Each is a difference of the neighbours on either side, with the centre row/column
weighted ×2. The edge strength is `sqrt(Gx² + Gy²)`; pixels above a threshold are painted black.

### First post-process pass

Everything so far ran inside `fragment()`, which has no pixel x/y and sees one pixel at a time.
Sobel needs a pixel's *neighbours*, so it can only run after the whole frame is rasterized, as a
loop over the framebuffer. This is the "real post-pass" the Lesson 10 blur row said it would need.

---

## Triage

| Concept | Bucket | How |
|---|---|---|
| Banding of the intensity | **Show** | Swap smooth for quantized diffuse and look |
| What gets quantized (diffuse only vs the summed light) | **Show** | Quantize diffuse first; if specular/shadow break the look, try the other |
| Sobel on depth | **Show** | Render the edge strength itself as greyscale before thresholding |
| Silhouette vs inner edges | **Show** | Background is cleared to depth 0, so the silhouette jump dwarfs inner ones |
| Threshold | **Measure** | Print min/max/typical edge strength over the frame, then pick |
| Kernel orientation / sign | **Discuss** | Magnitude uses squares, so sign and kernel transposition render identically |

---

## Pipeline

```
pass 1  DepthShader(light matrix)  → shadow_map_buffer     (unchanged, Lesson 9)
pass 2  DepthShader(camera)        → camera_depth_buffer   (unchanged, Lesson 10)
pass 3  toon shading               → framebuffer_ (colour + depth)
            per fragment: intensity = quantize(diffuse), colour = albedo × intensity
                          + white if Blinn-Phong specular > specular threshold
pass 4  outline post-pass          over framebuffer_
            for each interior pixel (x, y):
                Gx, Gy = Sobel over the 3×3 depths around (x, y)
                if sqrt(Gx² + Gy²) > threshold: pixel = black
```

Pass 4 reads depth and writes colour only, so it never reads a value it has already changed.

---

## Design decisions

| Decision | Choice | Status | Reason |
|---|---|---|---|
| Where quantization lives | New **`ToonShader`**, not a mode flag in `MaterialShader` | **Agreed** | One algorithm per file; `MaterialShader` stays the photoreal end-state and the A/B. |
| What `ToonShader` reads | Diffuse texture + tangent-space normal map + **shadow map**; no specular map, glow or AO | **Agreed** (shadow added at the header, user's call) | Smallest thing that shows bands on the real model; shadow and specular-map edges would fight the bands. Add terms back only if the screen asks. |
| Specular | **Thresholded Blinn-Phong highlight**: one on/off step, constant **white**, no specular map | **Agreed** | The user's call: the classic toon highlight is a flat spot, not a band. Colour is constant, not read from the map. Needs a shininess and a specular threshold. |
| Specular threshold / shininess | **0.5 / 100** | **Agreed** (on screen) | Spot visible and reads as a toon highlight; bands confirmed with `{0.2, 0.8, 1.0}`. |
| Levels | **Band values as a ctor parameter** (`band_values`, e.g. `{0.33, 0.66, 1.0}`); its size is the band count. Thresholds stay evenly spaced at 1/n; the band index selects an entry | **Agreed** (revised at spike, replaces `band_count`) | The user's call: evenly spaced values k/n floor the shadow at 1/n (33% at 3 bands), too light for deeper tones. Explicit values decouple tone depth from count. Band index: top-edge snap — `d` in ((k−1)/n, k/n] → band k. |
| Where the outline pass lives | Free function `Outline::drawOutlines` in **`src/rasterizer/postprocess/Outline.h/.cpp`** taking a `Framebuffer&` | **Agreed** | Not a shader — it has no vertex/fragment stages. `postprocess/` gives future post-passes (blur) a home. "Pass" dropped from the name — the folder already says it. |
| Depth source for edges | `framebuffer_`'s own z-buffer after pass 3 | **Agreed** | Already filled; no extra pass. `camera_depth_buffer` is identical but only exists for AO. |
| Border pixels | Skipped (loop `1 … size − 2`) | **Agreed** | No neighbours to read; one-pixel frame is invisible. |
| Threshold | **0.15** | **Agreed** (on screen) | Greyscale spike measured edge strength 0 – 3.11 (max = silhouette against the cleared depth 0); inner folds read as dark grey (~0.2–0.3). Swept up from 0.1 until slopes cleared and folds stayed. |
| Outline colour / width | **Colour as a parameter** (`outline_color`), final look a dark reddish `{15, 10, 12, 255}`; width 1 pixel | **Agreed** (on screen) | The user's call: a loud colour (e.g. red) makes misplaced or missing edges easy to spot while tuning. Width grows only if 1 px reads too thin. |

---

## Modules

### `src/rasterizer/shaders/ToonShader.h/.cpp` (new)

**Responsibility:** diffuse lighting quantized into bands, plus a flat white specular spot.

**API:**
- `ToonShader(const Mesh& mesh, const Framebuffer& shadow_map_buffer, const Texture& diffuse_texture, const Texture& normal_map_texture, const tinymath::Matrix4x4& transform, const tinymath::Matrix4x4& shadow_lookup_transform, tinymath::Vec3f light_direction, tinymath::Vec3f view_direction, std::vector<float> band_values, float specular_threshold, float shininess, float shadow_bias)`
- `tinymath::Vec4f vertex(int face_index, int vertex_index) override`
- `bool fragment(screen::BarycentricWeights weights, Color& out_color) override` — diffuse from the mapped normal, snapped to a level, times albedo; white added where the Blinn-Phong specular exceeds the threshold.

### `src/rasterizer/postprocess/Outline.h/.cpp` (new)

**Responsibility:** Sobel edge detection over a framebuffer's depth, painting edges black.

**API:**
- `void Outline::drawOutlines(Framebuffer& frame_buffer, Color outline_color, float threshold)`

### `Application`

- `testDrawMeshToonShader()` — shadow pass (no camera depth pass),
  then the toon pass into `framebuffer_`, then `Outline::drawOutlines`.

### Tests

None planned, same position as Lessons 8–10. Candidate if a bug resists the screen: `drawOutlines`
on a small framebuffer with a depth step — pixels along the step turn black, a flat depth stays untouched.

---

## Carried in from Lesson 10

- **Perspective-correct interpolation** is dropped (decided 2026-10-01): not needed for the learning goals; Lesson 11 closes the project.
- The `MaterialShader.h` doc comment still does not mention the normal map, shadow map and AO.
