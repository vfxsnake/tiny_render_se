# Lesson 9 — Shadow mapping

**Source:** https://haqr.eu/tinyrenderer/shadow/

> **Status: PLANNED (Session 58, 2026-09-21). No code written yet.**
> This document is the design agreed before the spike. The spike — rendering the shadow map to
> screen as greyscale — is the next action. Rows marked *Proposed* in the decisions table are
> Claude's recommendations that the user has not yet ruled on; rows marked *Agreed* were settled
> in discussion this session.

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

The fix is a small **bias** added when comparing (or subtracted when storing), so a surface is
not considered to block itself. The bias has two failure modes and both are visible:

- Too small → acne remains.
- Too large → **peter-panning**: the shadow detaches and slides away from the object casting it.

### Directional light, orthographic projection

A shadow map needs a *viewpoint*, but `lightDirection_` is only a direction. For a directional
light, the light camera is placed at `direction × distance` looking at the origin, and the
projection is **orthographic** — which in this codebase means `lookAt()` with **no**
`perspective()` matrix multiplied in, since `perspective(f)` is identity plus `−1/f`.

Under an orthographic projection the light's distance does not change the picture. It changes
only the depth range, which is what the bounds measurement below is about.

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
| Where the shadow term is applied | **Inside `MaterialShader::fragment()`**, not as a post-pass | *Proposed* | The shadow multiplies diffuse and specular but must **not** kill ambient or emission. A post-pass over the finished framebuffer cannot separate those components any more. |
| Shadow map resolution | Open | *Open* | Independent of the window. Changing one number answers whether a blocky shadow edge is resolution or bias. |
| Bias value | Open, found by sweeping | *Open* | Has no correct value a priori; both failure modes are visible on screen. |
| Shadow map lifetime | Local vs `Application` member | *Open* | Must outlive pass 1 and be readable during pass 2, so both passes must be reachable from one scope. |
| Back-face culling in the light pass | Left on | *Proposed* | Closed mesh; front faces as seen from the light are the correct occluders. |
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

### `Application::testDrawShadowMap()` (new — the spike)

**Responsibility:** the checkpoint-1 view. Builds the light matrix, renders every face with
`DepthShader` into a local `Framebuffer`, then blits that buffer's depth to the screen. Touches
`MaterialShader` not at all.

### `Application::blitDepthAsGreyscale(const Framebuffer& source)` (new)

**Responsibility:** reads `source.getDepth(x, y)` and writes a grey into `framebuffer_`.

Named and planned as a separate function from the start — per the Session 57 rule — because
Lesson 10 (SSAO) makes the depth buffer a shader input and will want this view again.

### Measurement, before any of the above is trusted

Loop the mesh vertices through the light matrix and print the min/max of light-space z. If the
range is not inside [−1, 1], the distance or a scale changes before the picture means anything.

### Tests

None planned. The pieces that could be tested in isolation are the depth comparison and the
bias, both inline in `fragment()`. Same position as Lesson 8 — revisit only if a bug resists
the screen.

---

## Open at the close of Session 58

- **Nothing implemented.** The next action is the spike: `DepthShader` +
  `testDrawShadowMap()` + `blitDepthAsGreyscale()`, preceded by the light-space z bounds
  measurement.
- **Light direction is still `{0,0,1}`.** That casts almost straight at the camera-facing side
  and will throw a shadow that is barely visible. Something like a normalized `{1,1,1}` gives an
  obvious cast shadow — but it changes the shading too, and `lookAt`'s up vector degenerates if
  the light direction becomes parallel to it.
- **`vertexWorldPosition_` is still object space.** This lesson adds a second transform (`N`)
  built from the same object-space convention, so the name stays wrong in the same way it was
  wrong in Lesson 8. A real model matrix is still the trigger to fix it.
- **UVs are still interpolated in screen space** (perspective-correct interpolation deferred to
  the end of the project). Do not misdiagnose a soft or offset shadow edge as a bias problem when
  it could be this.
