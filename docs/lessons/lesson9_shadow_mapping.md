# Lesson 9 — Shadow mapping

**Source:** https://haqr.eu/tinyrenderer/shadow/

> **Status: DONE (Session 60, 2026-09-27).** Both passes implemented; diablo renders with a hard
> cast shadow that tracks the light direction, acne cleared at `bias = 0.05f`. Every row in the
> decisions table is now **Agreed** — the three that were *Open* were closed this session.

## Goal

Add a global visibility test to the local Blinn-Phong model, so one part of the mesh can block
light from reaching another — the one thing a purely local shading model cannot express.

## Exit condition

Diablo rendered with `MaterialShader`, with a hard cast shadow under the horns, chin and arms
that moves correctly when the light direction changes, and **no acne stripes** on the lit
surfaces.

Intermediate checkpoints, in order:

1. **Shadow map visible.** The light-pass depth buffer, blitted to screen as greyscale, reads as
   diablo seen from the light's position. Nothing downstream can be trusted before this.
2. **Deliberate break — zero bias.** The full render with `bias = 0` shows shadow acne. This is
   looked at on purpose before it is fixed.
3. **End state.** Bias dialled in: acne gone, shadow still attached to the caster.

---

## Concepts

### Why Phong cannot cast a shadow

Blinn-Phong is *local*: the colour at a point depends only on the material, the light direction
and the view direction. Nothing in that calculation knows another triangle exists. A surface
facing the light is lit, full stop — even when something stands between it and the light. The
missing information is global visibility: *is there anything closer to the light along this ray?*

### The depth buffer answers exactly that question

A depth buffer rendered **from the light's viewpoint** stores, for every direction the light
looks, the distance to the nearest surface. That is a complete record of what the light can see.
A point is lit if it *is* that nearest surface, and shadowed if something else got there first.

So shadow mapping uses the depth buffer twice, in two passes:

1. **Light pass** — render the scene from the light. Keep only the depth buffer. This is the
   *shadow map*. The colour output is discarded.
2. **Camera pass** — render normally. For each fragment, transform it into the light's screen
   space and compare its depth against the stored one.

### Getting a fragment into light space

TinyRenderer's recipe takes the fragment in the *camera's* screen space and works backwards:
`N · M⁻¹ · f`, where `M` is the camera transform and `N` the light transform. It needs the
inverse because its shader does not keep the original position around.

**This project does not need the inverse.** `MaterialShader::vertex()` already stashes the
object-space position in `vertexWorldPosition_`. Interpolating that varying with the barycentric
weights gives the fragment's object-space position directly, and `N ·` that lands in light
screen space. No 4×4 inverse is written, and none needs a test.

### The comparison, and its sign

This project's depth convention: `viewport()` maps NDC z ∈ [−1, 1] → [0, 1], and the depth test
in `drawTriangle` is `getDepth(x, y) < depth` — **larger depth wins**, so larger means *nearer*.

Therefore the shadow map holds the **largest** depth = the surface closest to the light, and a
fragment is lit when its own light-space depth is **greater than or equal to** the stored value
(within a bias). Reverse the comparison and the image inverts: lit where it should be dark.

### Z-fighting and the bias

The lesson's named caveat. The fragment being shaded is, geometrically, the *same surface* that
wrote the shadow map — so the two depths should be equal. They are not, because the two passes
rasterize at different resolutions, sample different pixel centres, and round differently. Half
the pixels come out marginally behind their own record and classify themselves as shadowed.

The result is **shadow acne**: stripes of self-shadowing across lit surfaces, following the
rasterization pattern.

**The shape of the acne names its cause (observed, Session 60).** The first run produced *vertical
lines* across the model, not random speckle. Random speckle would mean floating-point noise between
two depths sampled at nearly the same place. A regular stripe means a *systematic* quantization
along x — which it was: `shadow_point_transformed.x/.y` were handed to `getDepth(int, int)` as
floats and truncated toward zero, so every lookup read a shadow-map pixel up to a whole pixel away
in one consistent direction, where the surface sits at a measurably different distance from the
light. `std::round` removed the stripes; the residual speckle is what the bias then handled.

The fix is a small **bias** added when comparing (or subtracted when storing), so a surface is
not considered to block itself. The bias has two failure modes and both are visible:

- Too small → acne remains.
- Too large → **peter-panning**: the shadow detaches and slides away from the object casting it.

### Directional light, orthographic projection

A shadow map needs a *viewpoint*, but `lightDirection_` is only a direction. For a directional
light, the light camera is placed along `direction` looking at the origin, and the
projection is **orthographic** — which in this codebase means `lookAt()` with **no**
`perspective()` matrix multiplied in, since `perspective(f)` is identity plus `−1/f`.

**Distance does nothing in this codebase (measured, Session 59).** `lookAt`'s `offset_matrix`
translates by `−target`, not `−eye`; the eye only contributes the direction `eye − target`. The
light-space origin sits at the target, so light-space z is `dot(p, direction)` whatever the
distance — the ranges at `× 2` and `× 3` were identical. The camera only works because
`perspective(3.0f)` supplies its distance. The only knob left for the depth range is a scale.

### The depth range trap

`viewport()` maps NDC z ∈ [−1, 1] → [0, 1], and `Framebuffer::clear()` sets every depth to
**0.0f**. A vertex whose light-space z falls below −1 maps to a *negative* depth, loses the test
against the cleared 0.0f, and is **silently never drawn**. The shadow map comes out with pieces
of the model missing and gives no indication why.

So the model's light-space z range must be measured and confirmed to sit inside [−1, 1] before
the shadow map is believed.

---

## Design decisions

| Decision | Choice | Status | Reason |
|----------|--------|--------|--------|
| Fragment → light space | Interpolate the existing object-space varying, apply `N` | **Agreed** | Avoids the 4×4 inverse entirely. An inverse we would have to write and test is a rung that buys nothing here. |
| Light type | **Directional** | **Agreed** | `lightDirection_` stays a constant, so shading and shadow are built from the same vector and cannot disagree. A point light would also make `lightDirection_` per-fragment — a second thing to get wrong while debugging the first. |
| Light projection | `lookAt()` only, no `perspective()` | **Agreed** | That *is* an orthographic projection in this codebase. No new math. |
| Shadow map storage | A second `Framebuffer`, CPU memory only | **Agreed** | `Framebuffer` already owns a `std::vector<float> depth_`. The colour half is written and ignored. Never uploaded to Vulkan — the display pipeline only ever sees `framebuffer_`. |
| Why not reuse `framebuffer_` | Two separate buffers | **Agreed** | The camera pass needs its own depth buffer for its own z-test. Sharing one would have pass 2's first `setDepth` destroy the light data it still needs. |
| Viewport matrix | Stays inside `drawTriangle`; the light matrix passed to the shader is `lookAt` only | **Agreed** | Pre-multiplying a viewport into the shader's transform would apply it twice. |
| Where the shadow term is applied | **Inside `MaterialShader::fragment()`**, not as a post-pass | **Agreed** | The shadow multiplies diffuse and specular but must **not** kill ambient or emission. A post-pass over the finished framebuffer cannot separate those components any more. |
| Shadow map resolution | `WIDTH × HEIGHT`, same as the screen | **Agreed** | One less suspect while pass 2 was being debugged: the lookup viewport comes out numerically identical to the camera's, so a misaligned shadow could not be blamed on a size mismatch. Built from `shadow_map_buffer.getWidth()/getHeight()`, **not** from `WIDTH`/`HEIGHT`, so changing it stays a one-line change. |
| Bias value | `0.05f` | **Agreed** | Swept from 0. No correct value a priori. |
| Shadow map lifetime | **Local to `testDrawMeshMaterialShader()`** | **Agreed** | Both passes live in that one function, so the buffer is provably alive while `MaterialShader` points at it. No `Application` member: nothing is interactive yet, so there is no reason to keep it across frames, and a member would put a raw lifetime dependency between two members. `MaterialShader` is *constructed* before pass 1 fills the buffer — harmless, since it holds a pointer and only reads in `fragment()`. |
| Back-face culling in the light pass | Left on | **Agreed** | Closed mesh; front faces as seen from the light are the correct occluders. |
| Light direction | Normalized `{1,1,1}` | **Agreed** | `{0,0,1}` casts straight at the camera-facing side and would barely show a shadow. |
| Fitting the model into the light frustum | Uniform `0.8` scale, `scale * lookAt(...)` | **Agreed** | Measured unscaled: x [−0.89, 0.84], y [−0.95, **1.045**], z [**−1.087**, 0.64]. z below −1 falls under the cleared 0.0f depth and vanishes; y above 1 clips the horns. Uniform so y is fixed too and proportions hold. At 0.8: z [−0.87, 0.51], y [−0.76, 0.84]. `data[3][3]` stays **1** — scaling w would be undone by the perspective divide. |
| Matrix passed to each pass | `DepthShader`: `light_matrix` only. `MaterialShader` lookup: `viewport(shadow map size) × light_matrix` | **Agreed** | `drawTriangle` applies the viewport internally in pass 1; the lookup in pass 2 has to reproduce it by hand, with the **shadow map's** size, not the screen's. |
| Which side the bias goes on | Added to the **fragment's** depth: shadowed when `z + bias <= stored` | **Agreed** | Equivalent to requiring `stored − z >= bias`, so a **bigger bias shadows less**. `z − bias` is the opposite and can never clear acne — it shadows *more* as the bias grows. Claude reviewed the `−` version as correct; the user corrected it. |
| Shadow-map pixel lookup | `static_cast<int>(std::round(...))` on x and y | **Agreed** | `getDepth` takes `int`. An implicit float→int conversion truncates toward zero, biasing every lookup up to a whole pixel in one direction — visible as *stripes*, not speckle (see the acne section). |
| Folding the viewport before the divide | Allowed: `viewport × light_matrix`, then `toVec3` | **Agreed** | `drawTriangle` applies its viewport *after* the divide, the lookup applies it *before*, yet the results are identical: the viewport's translation sits in column 3, which multiplies `w`, so dividing afterwards reproduces the offset exactly. The divide is therefore mandatory, not optional — and `tinymath::toVec3` already does it (asserting `w != 0`). |
| Soft shadows / PCF, multiple lights | **Out of scope** | **Agreed** | Deferred; not part of this lesson. |

---

## Modules

### `rasterizer/shaders/DepthShader.h/.cpp` (new)

**Responsibility:** the minimal shader for pass 1. Exists only so that `drawTriangle` runs and
fills a depth buffer; it produces no meaningful colour and carries no varyings.

**API:**
- `DepthShader(const Mesh& mesh, const tinymath::Matrix4x4& transform)`
- `tinymath::Vec4f vertex(int face_index, int vertex_index)` — transforms the object-space
  position and returns it. Stashes nothing.
- `bool fragment(screen::BarycentricWeights weights, Color& out_color)` — writes black, returns
  `true`. The rasterizer writes the depth.

### `rasterizer/shaders/MaterialShader.h/.cpp` (extended)

**Responsibility:** unchanged from Lesson 8, plus the light-visibility test.

**API change:**
- Ctor gains the shadow map as a non-owning `const Framebuffer*`, the light-space transform
  (`N`, including the light's viewport map), and the bias.
- `fragment()` interpolates `vertexWorldPosition_`, transforms it by `N`, does the perspective
  divide, samples `shadowMap_->getDepth()` at the resulting pixel, and compares.
- The resulting factor multiplies **diffuse and specular only**.

**As implemented:**

```
shadowed  ⟸  shadow_point_transformed.z + shadowBias_ <= shadowMapFramebuffer_->getDepth(
                  round(shadow_point_transformed.x), round(shadow_point_transformed.y))
```

with `shadow_multiplier` ∈ {0, 1} multiplying `diffuse` and `specular` in all three colour
channels. Ambient and emission are left untouched, so shadowed regions keep the material's
ambient term and the glow map still reads through them.

### `Application::testDrawMeshShadowMap()` (the checkpoint-1 spike, implemented)

**Responsibility:** the checkpoint-1 view. Builds the light matrix, renders every face with
`DepthShader` into a local `Framebuffer`, then blits that buffer's depth to the screen. Touches
`MaterialShader` not at all.

### `Application::blitDepthAsGrayscale(const Framebuffer& source)` (new, implemented)

**Responsibility:** reads `source.getDepth(x, y)` and writes a grey into `framebuffer_`.

Named and planned as a separate function from the start — per the Session 57 rule — because
Lesson 10 (SSAO) makes the depth buffer a shader input and will want this view again.

### Measurement, before any of the above is trusted (done, Session 59)

Looped the mesh vertices through the light matrix and tracked per-axis min/max. The unscaled
range failed on z and y; distance was shown to be a no-op; a uniform 0.8 scale brings every axis
inside [−1, 1]. See the decisions table.

### Tests

None planned. The pieces that could be tested in isolation are the depth comparison and the
bias, both inline in `fragment()`. Same position as Lesson 8 — revisit only if a bug resists
the screen.

---

## Close-out (Session 60)

Both checkpoints hit in order, and both deliberate breaks were seen on screen:

- **Inverted compare.** The first working comparison had lit and shadowed swapped; flipping it
  fixed the image. The triage bucket said *show it*, and it showed.
- **A false positive worth remembering.** Before the comparison used the fragment's own depth at
  all, the code tested the *stored* depth against the bias (`stored < 0.5`). That darkened the far
  half of the light's depth range, which correlates with facing away from the light — so it read as
  a plausible shadow. With the bias then set to 0 the test became `stored < 0`, never true, and the
  render showed **no shadow at all** while still looking correct, because ordinary `N·L` falloff
  plus ambient is what a shadow is easy to mistake for. The check that settles it: *does a horn cast
  a dark patch onto the chest?* A test that never compares the fragment against the occluder cannot
  produce that, whichever way its branch points.
- **Acne at zero bias**, diagnosed from its stripe pattern rather than assumed (see the acne
  section), then cleared at `0.05f`.
- **Tracking confirmed.** Changing `light_direction` moves the cast shadow with the shading, since
  the one vector feeds both the light matrix and `lightDirection_`.

Closed this session: shadow-map lifetime, resolution, bias value, in-shader vs post-pass,
back-face culling — all now in the decisions table. The light direction in
`testDrawMeshMaterialShader()` is now the same normalized `{1,1,1}` as the light pass, and
`emission`/`ambient`, zeroed while isolating the shadow, are restored to `1.0f`/`0.1f`.

### Still carried forward

- **The 0.8 scale is tuned for `{1,1,1}` specifically.** A different light direction changes the
  light-space bounds, so the depth-range trap can return. If a shadow goes partly missing after a
  direction change, that is the first suspect, not the bias.
- **`vertexWorldPosition_` is still object space.** This lesson adds a second transform
  built from the same object-space convention, so the name stays wrong in the same way it was
  wrong in Lesson 8. A real model matrix is still the trigger to fix it.
- **UVs are still interpolated in screen space** (perspective-correct interpolation deferred to
  the end of the project). Do not misdiagnose a soft or offset shadow edge as a bias problem when
  it could be this.
