# Lesson 6 — Shading

**Source:** https://haqr.eu/tinyrenderer/shading/

> **Status: in progress (started 2026-08-19, doc written 2026-08-24).**
> Written three sessions late — the spike ran first (flat world-space lighting, the screen-space
> normal break, the `vn` loader extension) and this document records what those spikes earned as
> well as the design still to be built. Everything in *Prior findings* already happened; everything
> in *Modules* is still ahead.

## Goal

Move shading out of the vertex loop and behind a programmable interface, so that what a pixel's
colour *means* is decided by a shader object rather than by the rasterizer — and use that interface
to climb flat → Gouraud → Phong with all three rungs alive side by side.

## Exit condition

Three shaders render the same mesh through the same rasterizer call:

- `FaceShader` reproduces the picture `testDrawMeshMatrixLightWorldSpace()` already produces, pixel
  for pixel. If it differs, the plumbing is wrong, not the shading.
- `GouraudShader` shades from the file's `vn` normals with a per-vertex intensity blended across
  the triangle — visibly smoother on the silhouette-adjacent faces.
- `PhongShader` interpolates the *normal* instead of the intensity and adds ambient + specular.
  With the specular exponent turned up, a highlight lands mid-triangle under Phong and **vanishes
  entirely** under Gouraud. That disappearing highlight is the lesson's payoff and the reason the
  scope runs this far.

`RasterVertex` is unchanged at the end of the lesson — no varying was ever added to it.

---

## Prior findings (sessions 38–40, recorded here retroactively)

### The screen-space normal break, and why it failed harder than predicted

The flat shader was deliberately broken by computing the face normal from the *projected*
coordinates instead of the world ones. The registered prediction was that a single frame would look
plausible and the lighting would *swim* as the camera orbited. What actually appeared was a
**uniformly flat white model**.

The cause is a **unit mismatch, not a geometric one.** `viewport()` scales x and y by
`half_width`/`half_height` — a few hundred — and z by `0.5`. A triangle edge in that space therefore
has Δx, Δy in *pixels* and Δz in *thousandths*. In `cross(b - a, c - a)` the z component is
`Δx₁Δy₂ − Δy₁Δx₂` (pixels², order tens) while the x and y components each carry a Δz factor (order
thousandths). The z term wins by roughly 10³–10⁴, so after `normalize` every normal is `{0, 0, ±1}`
to several decimals, `dot` with the light is ±1 everywhere, `max(0, ·)` plus back-face culling kills
the −1s, and every surviving face is full white. **The swim never got a chance to appear — it needs
surviving variation to swim, and the unit mismatch flattened all of it first.**

**The invariant, sharpened:** the normal, the light direction and the view direction must be in the
*same space* — and "same space" includes **the same units per axis**, not merely the same
orientation.

### Why a rasterizer carries attributes forward rather than reconstructing them

Asked whether lighting could work by going from the 2D pixel *back* to its 3D position: for a ray
tracer that is right, and there is no "going back" because you never left — the hit point is already
a world position. For a rasterizer the projection is **destructive and not invertible per pixel**:
many 3D points collapse to one 2D point and the `w` divide throws the scale away. So the answer is
not "go back to 3D", it is **carry the 3D forward** — the vertex stage keeps the attributes
attached, the rasterizer computes weights in 2D and blends the attributes with them.

That is what a varying *is*. (Reconstructing world position from stored depth plus the inverse
view-projection is a real technique — it is deferred shading — and it works precisely because depth
was stored.)

### Measurement: the model already ships normals

`diablo3_pose.obj` carries `vn` lines and they are already unit length. That settled the fork:
the loader stops discarding the third index of every `v/vt/vn` triad, rather than the program
computing vertex normals by averaging face normals.

---

## Vocabulary

This project sits between three vocabularies — graphics literature, USD, and Houdini — and two words
collide outright. The table is the agreed mapping; **the Houdini/USD column is the one to use in
conversation**, because it is the jargon already in the user's head.

| This renderer | USD interpolation | Houdini class | Meaning here |
|---|---|---|---|
| per-face (`FaceShader`) | `uniform` | **primitive** attribute | one lighting value per triangle, constant across it |
| per-corner (`GouraudShader`) | `faceVarying` | **vertex** attribute | one value per face-corner, interpolated across the triangle |
| per-point | `varying` / `vertex` | **point** attribute | one value per shared point |
| whole mesh | `constant` | **detail** attribute | one value for everything |
| per-pixel (`PhongShader`) | — | — | no equivalent — this is a *shading rate*, not an attribute class |

**Three traps, in order of how likely they are to bite:**

1. **GLSL "varying" ≠ USD `varying`.** In shader-speak a *varying* is **any** per-vertex output the
   vertex stage writes and the fragment stage reads interpolated — it says nothing about points
   versus corners. USD's `varying` specifically means per-*point*. This document uses the GLSL sense
   throughout, because that is what the interface implements.
2. **Houdini's "vertex" is USD's `faceVarying`, not USD's `vertex`.** Houdini vertex = face-corner;
   USD vertex = point. Mixing the two vocabularies without noticing this inverts the meaning.
3. **The `vn` normals in the OBJ are per-corner, not per-point** — a Houdini **vertex** attribute.
   `faceNormalIndices` exists precisely because the normal index is independent of the position
   index. So `GouraudShader` is *not* point shading, and hard edges in the model work exactly
   because one point carries different normals in different faces.

**"Flat" is kept for the rate** in written material, since that is what the literature and the
source page mean by it — but the class is `FaceShader` and the spoken word is *primitive*.

## Concepts

### The pipeline, and which stage owns what

Four stages, in order: **vertex** (per corner: fetch attributes, transform position), **clipping and
the perspective divide**, **rasterization** (which pixels are covered, and with what weights), and
**fragment** (what colour a covered pixel gets). A shader is programmable at exactly two of those —
vertex and fragment. The two in the middle are fixed rules.

### Uniform vs varying

Both are shader member variables. The difference is lifetime:

- A **uniform** is set once before the draw and read by every invocation — the transform matrix,
  the light direction, the specular exponent.
- A **varying** is written by `vertex()` for one corner and read by `fragment()` after being blended
  across the triangle — the intensity, the normal, later the uv.

Naming them apart is what keeps them straight; C++ gives you no help.

### Why varyings live on the shader and not on `RasterVertex`

`RasterVertex` is the *geometric* payload — what the rasterizer needs to scan-convert: screen
position and depth. An intensity is a shading result, a different owner. Putting a varying slot on
`RasterVertex` would grow it a field per lesson (`intensity`, then `normal`, then `uv`), and that
growth is the argument against it.

The tension is that the rasterizer is the only place that has the barycentric weights, so *something*
has to be blended there. The shader interface resolves it: `vertex()` writes
`varying_x_[nth_vertex]` as a side effect, `fragment()` reads it, and the rasterizer passes only the
weights — **it never learns what is being blended.**

### Clip space, and why the divide is not the shader's job

`transformation * toVec4(p)` yields a `Vec4f` whose `w` is generally not 1. That is **clip space**.
Dividing by `w` gives NDC; the viewport transform then maps NDC to pixels.

A GLSL vertex shader writes `gl_Position` in clip space and stops. The divide is fixed-function
hardware, in OpenGL, Vulkan, D3D and Metal alike, **because clipping has to happen before the
divide.** A vertex behind the eye has `w ≤ 0`, and dividing by it produces garbage or a sign flip
that wraps geometry across the screen. The hardware clips in clip space first — where the test is a
clean set of linear comparisons of `x, y, z` against `±w` — and divides only the survivors.

This renderer does not clip yet, which is why the current divide-in-the-loop has never misbehaved.
Walking the camera close enough that a triangle straddles the eye plane will expose it.

### Discard

`fragment()` returns `bool`. `false` means *do not write this pixel* — no colour, no depth. Nothing
in this lesson needs it, but it is free here and it is the mechanism behind alpha cut-outs and the
toon-shading outline pass later.

### Flat, Gouraud, Phong

The same three-step recipe at three different rates:

| | normal source | `max(0, n·l)` evaluated | what is interpolated |
|---|---|---|---|
| Face (flat) | cross product of the face's edges | once per **face** | nothing |
| Gouraud | the file's `vn`, one per corner | once per **vertex** | a `float` intensity |
| Phong | the file's `vn`, one per corner | once per **pixel** | a `Vec3f` normal |

Gouraud's failure mode is the whole argument for Phong: a specular highlight that lands in the
middle of a triangle is never evaluated at any of its three vertices, so it simply **does not
exist**. Under Phong the normal is reconstructed per pixel and the highlight appears.

### "Phong" names two different things

- **Phong *shading*** — interpolate the normal, evaluate lighting per pixel. That is a *rate*.
- **Phong *reflection model*** — the formula `ambient + diffuse + specular`. That is *what* is
  evaluated.

They are independent: the reflection model can be run at flat rate, and Lambert-only can be run
at Phong rate. This lesson adopts both at the same time, which is why they are easy to conflate.

### The Phong reflection model

`colour = ambient + diffuse + specular`, all from uniforms — **no texture sampling anywhere in this
lesson.** (Specular *maps* belong to the next lesson, "More data!".)

- **Ambient** — a constant floor, standing in for light that bounced off everything else. Without
  it, unlit faces are pure black, which reads as a hole rather than as shadow.
- **Diffuse (Lambert)** — `max(0, n·l)`. Scatters equally in all directions, so it does not depend
  on where the viewer is.
- **Specular** — `max(0, r·v)ᵉ`, where `r` is the light reflected about the normal and `v` points at
  the viewer. The exponent `e` controls tightness: low is a broad sheen, high is a small hard dot.
  This is the only term that moves when the camera moves.

---

## Design decisions

| Decision | Choice | Reason |
|----------|--------|--------|
| Face normal indices storage | A second parallel array `faceNormalIndices` beside `faceIndices` | User's explicit call over the recommended `FaceVertex { position; normal; }`, on the grounds of not over-engineering a side topic. Matches TinyRenderer's own `facet_vrt`/`facet_nrm`. **Revisit when `vt` lands** — a third parallel array is the trigger. |
| Normal source | The file's `vn` lines | Measured: `diablo3_pose.obj` ships them, already unit length. Averaging face normals would be work to reproduce data already present. |
| First varying type | `float` intensity (Gouraud) before `Vec3f` normal (Phong) | It is the naive rung and **its breakdown is the argument for Phong**. Also isolates one new mechanism, so a wrong picture has one suspect. Both rungs stay alive per the keep-superseded-rungs rule. |
| Where the varying lives | Shader member, not `RasterVertex` | See *Why varyings live on the shader*. The rasterizer stays ignorant of shading. |
| `vertex()` return space | **Clip space** (`Vec4f`, undivided) — the rasterizer does the divide and the viewport | Matches OpenGL/Vulkan exactly, and puts the divide where clipping will eventually have to live. The rasterizer already knows the framebuffer dimensions, which is what `viewport` needs. |
| Viewport removal from the app matrix | `Application` composes `perspective · lookAt` only | Follows from the above. **Numerically a no-op**: `viewport` is affine and does not touch `w`, so applying it before or after the `w` divide gives the same result. Pure refactor, no picture change — which is what makes `FaceShader` a valid A/B. |
| New rasterizer entry point | `drawTriangleWithShader`, added **beside** `drawTriangle` | Keep-superseded-rungs: the flat-colour path stays callable as a standing comparison and as a benchmark baseline. |
| `RandomShader` rung | Kept, as step 3.5 before `FaceShader` | Isolates the plumbing from the shading: five suspects instead of six on a wrong picture, and the correct image is already known from Lesson 3. A constant/unlit model at primitive rate — the control that changes one axis at a time. |
| Lesson scope | Ends at Phong + specular | The source lesson runs that far and every term is uniform-only. Gouraud vs Phong is unpersuasive under pure diffuse; the vanishing highlight needs the specular term. |
| `AbstractShader` ownership | Abstract base class with two pure virtuals; shaders are stack objects at the call site | Three implementors exist by the end of the lesson, so the interface is earned rather than premature. |
| Shader file location | `src/rasterizer/shaders/` | The shaders speak the rasterizer's vocabulary (`screen::BarycentricWeights`, `Color`) and the rasterizer calls them — one component, not two. A bare `src/shaders/` would also collide with the repo-root `shaders/` holding the Slang GPU source (`CMakeLists.txt:101`). `materials/` was considered and rejected: the shadow-mapping depth pass is an implementor but not a material. |
| Flat shader naming | `FaceShader`, not `FlatShader` or `PrimShader` | Matches the user's jargon. `PrimShader` rejected — "primitive shader" is an existing, different GPU pipeline stage. The literature term is pinned in a header comment. |
| Interim rasterizer name | `drawTriangleWithShader` for now | The shaded path is the *terminal* signature — textures, tangent space, shadow mapping and SSAO are all shader-internal or extra passes, and perspective-correct interpolation gets `w` for free from the clip positions. So it eventually deserves the plain `drawTriangle` name and today's flat-colour version deserves the qualifier. **That rename is deliberately deferred to the end of the lesson** rather than churning call sites mid-build. |

---

## Modules

### `rasterizer/AbstractShader.h`

**Responsibility:** the programmable boundary. Owns nothing; declares the two hooks the rasterizer
calls. Header-only — it is pure interface.

**API:**
- `virtual ~AbstractShader() = default` — base class deleted through a base pointer is not the
  intended usage, but the destructor is virtual anyway so it cannot become a trap.
- `virtual tinymath::Vec4f vertex(int face_index, int nth_vertex) = 0` — fetch corner
  `nth_vertex` (0–2) of face `face_index`, write any varyings for that corner, return the
  **clip-space** position.
- `virtual bool fragment(screen::BarycentricWeights weights, Color& out_color) = 0` — blend the
  varyings with `weights`, write `out_color`, return `false` to discard.

### `rasterizer/TriangleRasterizer.h/.cpp` — additions

**Responsibility:** unchanged — coverage, weights, depth test. It gains the divide and the viewport
because it is the only place that knows the framebuffer's dimensions.

**API:**
- `void drawTriangleWithShader(const std::array<tinymath::Vec4f, 3>& clip_positions, AbstractShader& shader, Framebuffer& frame_buffer, bool cull_back_faces = true)`
  — divides each clip position by `w`, applies `viewport(fb.getWidth(), fb.getHeight())`, builds a
  screen-space `Triangle` internally, then runs the existing body: `twiceSignedArea` → cull →
  `boundingBox` → per-pixel `barycentricWeights` → depth test → `shader.fragment(...)` → `setPixel`.

The existing `drawTriangle(const Triangle&, Color, ...)` is untouched and stays in use.

### `rasterizer/shaders/RandomShader.h/.cpp`

> **Step 3.5 — the plumbing proof.** A constant/unlit *model* running at primitive *rate*: one
> arbitrary colour per triangle, no lighting arithmetic anywhere.

**Responsibility:** exercise the interface with nothing that can be arithmetically wrong. Five new
mechanisms land at once with the shader path — `vertex()` called three times per face with the right
indices, clip positions coming back correct, the rasterizer's new divide, its new viewport, and
`fragment()`'s return reaching the framebuffer. A first shader that also does lighting gives a wrong
picture six suspects instead of five.

**Why random and not one constant colour:** a single colour renders the mesh as a featureless
silhouette, where a per-triangle geometry error hides completely. Random per face makes every
triangle individually visible, so a broken divide or viewport shows immediately as scrambled or
misplaced facets. **The target image is already known** — it is the confetti render from Lesson 3,
made before the z-buffer existed.

**Uniforms:** `const io::Mesh* mesh_`, `tinymath::Matrix4x4 transform_`.
**Varyings:** `Color faceColor_`.

`vertex()` does the full transform — the half being tested — and picks the face's colour on the
first call. `fragment()` ignores the weights and writes it. Note this is *not* a flag on
`FaceShader`: `FaceShader`'s exit condition is reproducing the world-space grey picture pixel for
pixel, and random colours would destroy that comparison. Two shaders, one axis changed at a time.

### `rasterizer/shaders/FaceShader.h/.cpp`

> One lighting evaluation per face, constant across the triangle. Known as **flat shading** in
> the literature and in the TinyRenderer source — named `FaceShader` here to match the project's
> own jargon, with the standard term pinned in the header so it stays greppable against
> outside references.

**Responsibility:** reproduce the current flat picture through the new path. The control in the A/B.

**Uniforms:** `const io::Mesh* mesh_`, `tinymath::Matrix4x4 transform_`, `tinymath::Vec3f lightDirection_`, `Color baseColor_`.
**Varyings:** `tinymath::Vec3f varyingWorldPosition_[3]`, `float faceIntensity_`.

`vertex()` stores the world position of each corner; on the third call it computes
`normalize(cross(b - a, c - a))` and `faceIntensity_ = max(0, n·l)`. `fragment()` ignores the
weights entirely and scales `baseColor_`.

### `rasterizer/shaders/GouraudShader.h/.cpp`

**Uniforms:** same as flat.
**Varyings:** `float varyingIntensity_[3]`.

`vertex()` fetches the corner's normal via `faceNormalIndices` and stores
`max(0, n·l)`. `fragment()` blends the three floats with the weights and scales `baseColor_`.

### `rasterizer/shaders/PhongShader.h/.cpp`

**Uniforms:** flat's, plus `tinymath::Vec3f viewDirection_`, `float ambient_`, `float shininess_`.
**Varyings:** `tinymath::Vec3f varyingNormal_[3]`.

`vertex()` stores the raw normal. `fragment()` blends the three normals, **re-normalizes** (a
barycentric blend of unit vectors is not unit), reflects the light about it, and sums ambient +
diffuse + specular.

### `Application` — call sites

`testDrawMeshMatrixLightWorldSpace()` stays — it is the reference `FaceShader` must reproduce.

**`testDrawMeshMatrixLightNormalsFromFile()` is deleted at step 6**, once its size check has moved
into `ObjLoader` as the validation throw. This does *not* violate keep-superseded-rungs: that rule
protects working *rungs* kept as standing A/Bs, and this function never rendered anything the flat
one did not — it is a copy of it plus a guard, and it still computes the face normal by cross
product rather than reading the file normals its name claims. Verification scaffolding, not a rung. Three new functions, one per shader, each composing `perspective · lookAt` **without**
`viewport` and looping `for face → for 3 corners → shader.vertex(...)` → `drawTriangleWithShader`.

### `tests/test_shading.cpp`

**Cases:**
- A varying evaluated at each of the three corners returns that corner's value exactly
  (weights `{1,0,0}`, `{0,1,0}`, `{0,0,1}`).
- A varying at the centroid (`{1/3, 1/3, 1/3}`) returns the mean of the three.
- `FaceShader` and a hand-computed `max(0, n·l)` agree for a known triangle and light.
- A blended normal from three differing unit normals is not unit length before re-normalization —
  pins the reason `PhongShader::fragment` normalizes.
- `fragment()` returning `false` leaves both the colour and the depth buffer untouched.

---

## Deferred to the end of this lesson

The five `ObjLoader` review findings, queued by explicit choice so the loader work was not
interrupted:

1. **The parser assumes full `v/vt/vn` triads, and silently produces garbage when it is wrong**
   (the only correctness item). `f 1//1 2//2 3//3`, `f 1/1 2/2 3/3` and `f 1 2 3` all break
   identically: `iss >> skip_int` hits `/`, fails, sets failbit — and since C++11 a failed
   extraction **writes 0 to the target**, so every later read is a no-op leaving 0 behind. Indices 1
   and 2 become 0, then `-= 1` makes them **−1**, and downstream that is `mesh.vertices[-1]`: UB, no
   throw, no warning.

   **The fix is not a `throw`.** `f 1//1 2//2 3//3` — positions and normals, no texture
   coordinates — is *legal* OBJ and exporters emit it routinely; rejecting it would reject valid
   files. The parser must read the triad **by structure** (count the slashes, see which fields are
   actually present) rather than assuming a shape and failing silently when surprised. Two distinct
   jobs come out of that, and they must not be conflated:

   - **Presence detection** — a missing `vt` is a legal file with no texture coordinates. Record
     what the mesh actually has, so a later texture shader can ask (`mesh.hasUVs()`) and fail loudly
     at the call site instead of silently indexing an empty vector. This is what makes the texture
     lesson safe.
   - **Validation** — an index pointing outside `vertices` / `normals` / `uvs` is a corrupt file,
     with no legal reading. That one throws.
2. **No index range validation** — nothing checks against `vertices.size()` / `normals.size()`.
   This is the *validation* half above, and it is where the size check currently living in
   `Application::testDrawMeshMatrixLightNormalsFromFile()` belongs permanently — moving it here
   removes that function's reason to exist.
3. **`iss(line.c_str())`** — `line` is already a `std::string`; `iss(line)` avoids a scan and a copy.
4. **`iss>> normal_indices[1]`** — missing space, the only instance in the file.
5. **Six hand-written `-= 1`s** — a small local lambda would collapse the block and make the
   `/`-skipping structure visible instead of buried in repetition.

---

## Open questions

- ~~Whether to add a `RandomShader` rung~~ — **decided: yes, step 3.5.** See its module section.
- **Whether `GouraudShader` / `PhongShader` should also be renamed to rate-based names** (vertex /
  pixel). Left as-is for now: unlike "flat", those two are eponyms rather than descriptions, so they
  carry less ambiguity to correct.
