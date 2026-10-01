# Lesson 10 — Ambient occlusion

**Source:** https://haqr.eu/tinyrenderer/ssao/

> **Status: COMPLETE (Session 63, 2026-09-30).** Both exit conditions seen on screen. Final
> values: radius 0.2, N 256, bias 0. Timing, screen-edge lightening and the `{-1, 1, 1}` shadow
> check were **skipped** by the user's call; range check and blur were not needed.

## Goal

Replace the constant ambient term with a geometry-aware one: estimate, per fragment, how much of
the surrounding hemisphere is blocked, using only the camera's depth buffer (SSAO).

## Exit condition

1. **AO-only view.** A greyscale render of the AO factor alone, where crevices (armpits, under the
   horns, eye sockets, cloth folds) are clearly darker than open surfaces.
2. **End state.** The same factor darkens the ambient term in the full `MaterialShader` render.

---

## Scope

The lesson has two homeworks. **Only SSAO is built.**

- **Brute-force AO** (≈1000 shadow maps from directions over a hemisphere, averaged) — **skipped**,
  neither as a rung nor as an answer key. Decided by the user under the Lesson 7+ rung policy.
- **SSAO** — built directly in its end form: samples in a **normal-oriented hemisphere**, not a
  sphere.

---

## Concepts

### Why a constant ambient is wrong

In Blinn-Phong the ambient term is a constant: every point receives the same amount of indirect
light, whether it sits on an open shoulder or deep in an armpit. Real indirect light arrives from
all directions over the hemisphere above a point, and nearby geometry blocks part of it. Ambient
occlusion estimates *the fraction of that hemisphere that is open*, and scales the ambient term by it.

### Why only depth is needed

The depth buffer is a record of the nearest surface along every camera ray. Pick a point near the
fragment, project it to the screen, and compare its depth with the stored one:

- stored depth is **nearer** than the point → something is in front of it → the point is **inside
  geometry** → that direction is blocked;
- otherwise → open.

The fraction of open samples is the AO factor. This is the shadow-map comparison from Lesson 9,
with the *camera's* depth buffer and many sample points instead of one light.

### Why a hemisphere oriented by the normal

Samples from a full sphere around a point on a flat surface land half above and half below it;
the half below is always "inside geometry", so every flat surface reads ≈50% occluded. Keeping
only the hemisphere on the normal's side makes a flat, open surface read fully open. The sphere is
not built — this is why.

### Which normal orients it

The **normal-mapped normal** from `MaterialShader` (agreed). Known risk, to *show*, not pre-fix:
the depth buffer holds only the low-poly geometry. Where the mapped normal tilts far from the
geometric one, part of the hemisphere dips below the real surface in the depth buffer, and the
fragment darkens itself in a pattern that follows the texture, not real crevices. If that shows,
the A/B is swapping in the interpolated vertex normal.

### Depth convention (unchanged from Lesson 9)

`viewport()` maps NDC z to [0, 1] and **larger depth = nearer**. The buffer is cleared to `0.0f`,
and `Framebuffer::getDepth` returns `0.0f` out of bounds. So a sample is occluded when
`stored_depth > sample_depth (+ bias)`, and neither the background nor off-screen can ever occlude.
That last fact is the lesson's **screen-edge lightening** artifact, predicted from the code.

The depth after the perspective divide is non-linear in distance but monotonic, so the comparison
*order* is correct. Only the bias is in non-linear units.

---

## Triage

| Concept | Bucket | How |
|---|---|---|
| Core test (sample, project, compare, average) | **Show** | The AO-only view |
| Self-occlusion "acne" (samples in the tangent plane of their own surface) | **Show** | Bias at 0 first, same as Lesson 9 |
| Sampling radius | **Show** | Sweep: too small shows nothing; too large darkens against unrelated geometry and makes halos |
| Sample count, fixed kernel → banding vs noise | **Show** | Sweep N |
| Mapped-normal self-darkening | **Show** | Look for texture-shaped darkening on open areas |
| Screen-edge lightening | **Show** | Predicted above; confirm at the borders |
| Radius units | **Measure** (already known) | Samples live in object space; Lesson 9 measured the mesh at ≈ [−1, 1] per axis, so a radius is a fraction of that |
| Cost per frame × N samples | **Measure** | Time the full render at a few N |
| How strongly AO shows in the final render | **Discuss → Show** | Ambient is `0.1` today; AO on ambient only may be near-invisible. Decided after the AO-only view |

---

## Pipeline

```
pass 1  DepthShader(camera transform)   → camera_depth_buffer        (depth pre-pass)
pass 2  DepthShader(light matrix)       → shadow_map_buffer          (unchanged, Lesson 9)
pass 3  MaterialShader                  → framebuffer_
            per fragment:
              P  = interpolated object-space position
              n  = mapped normal (already computed for lighting)
              for each occlusion sample vector k:
                  s   = P + orient(k, n) * radius
                  s'  = ssao_lookup_transform * s, divided   → screen x, y, depth
                  occluded if camera_depth_buffer(round x, round y) > s'.z + bias
              ao = open / N
              ambient term *= ao
```

The AO-only checkpoint is this same pass with only the ambient term switched on (see the decisions table).

---

## Design decisions

| Decision | Choice | Status | Reason |
|----------|--------|--------|--------|
| Technique | SSAO only; brute-force AO skipped | **Agreed** | Rung policy; brute force is a different technique, not a step toward SSAO. |
| Sampling domain | Normal-oriented hemisphere | **Agreed** | A sphere reads flat open surfaces as ≈50% occluded. |
| Normal source | Normal-mapped normal | **Agreed** | User's choice. Mapped-normal self-darkening is a known risk to *show*; vertex normal is the A/B. |
| Where AO is computed | **Inside the fragment shader**, not a screen post-pass | **Agreed** | `fragment()` has no pixel x/y, so it cannot index a per-pixel G-buffer — but it *has* the object-space position and the mapped normal already. Projecting samples with a lookup matrix is exactly the Lesson 9 shadow-lookup mechanism. This removes the need for a **normal buffer**, a **position buffer** and any **matrix inverse** (reconstructing position from depth). Supersedes the "normal buffer" mentioned during kick-off. |
| Space the samples live in | **Object space** (same as `vertexWorldPosition_`) | **Agreed** | The mapped normal is already in object space, and the radius gets meaningful units (mesh ≈ [−1, 1]). |
| Scene data SSAO reads | A camera **depth pre-pass** into its own `Framebuffer` (`DepthShader` reused with the camera transform) | **Agreed** | Pass 3 is still filling its own z-buffer while it shades; a fragment drawn early would read an incomplete buffer. The pre-pass gives every fragment the finished depth. Same reason Lesson 9 kept two buffers. |
| SSAO lookup matrix | `viewport(camera depth buffer size) × transformation_matrix`, then `toVec3` (divides) | **Agreed** | Identical construction to the shadow lookup; the fold-before-divide equivalence from Lesson 9 applies unchanged. |
| Pixel lookup | `std::round` on x and y | **Agreed** | Lesson 9: truncation shows as stripes. |
| Sample kernel | `N` random vectors inside the unit sphere, generated **once** in the constructor with a fixed-seed `std::mt19937`; per fragment, flipped into the normal's hemisphere (`dot(k, n) < 0 → −k`) | **Agreed** | No tangent frame needed to orient a kernel — the flip does it. Fixed seed keeps renders reproducible. Fixed kernel → banding at low N rather than noise; that is a *show* item, and per-fragment randomisation is only added if the banding says so. |
| Name of the sample set | **`occlusion_sample_vectors`** (parameter), **`occlusionSampleVectors_`** (member) | **Agreed** | Not "kernel" — the SSAO literature's term, but it hides what the thing is. Each entry is a *vector*: a direction (flipped into the hemisphere) **and** a length (how far inside `radius` the sample lands). Not "hemisphere": stored as a full sphere, it only becomes a hemisphere after the per-fragment flip. `occlusion` disambiguates from texture sampling inside `MaterialShader`. |
| Where AO is applied | Multiplies the **ambient term only**, for now | **Agreed** | That is what AO models; diffuse/specular already have direct visibility from the shadow map; emission is self-lit. The user's prior experience: AO multiplied over the *whole* composite (the common black-and-white AO pass) added a dusty look in places and washed out others. So it starts as an ambient-only component; whether it should also modulate the other components is decided after seeing it on screen. |
| Shared code | **`estimateAmbientOcclusion`** as a dedicated free function in `AmbientOcclusion.h/.cpp`. The tangent-space block stays inline in `MaterialShader::fragment()` for now | **Agreed** | The user asked for AO as a dedicated function, not inline in `fragment()`; one algorithm per file rather than a `ShaderUtils` catch-all. Extracting the tangent-space normal is deferred to the end of the lesson, and only if the user sees fit — otherwise it stays as is. |
| AO-only view | **No new shader.** `MaterialShader` itself, with diffuse, specular and emission intensities at `0` and ambient at `1`. For pure AO, pass a **1×1 white `Texture`** (built in memory — `Texture(pixels, 1, 1)`) as the diffuse input; the real diffuse texture shows albedo × AO | **Agreed** | Same code path as the final render, so the checkpoint cannot disagree with it. AO *is* the ambient contribution, so isolating ambient isolates AO — no temporary debug output. The white texture needs no shader change, since ambient = diffuse colour × ambient × AO. |
| Radius | **0.2** (object units) | **Settled** | Swept on screen (Session 63): 0.1 darkened the wrinkles; 0.2 makes cavities read more strongly. Larger radius finds more occluders. |
| Sample count `N` | **256** | **Settled** | Raised from the lesson's 128 (Session 63): steadier darkening, not stronger. Never timed (skipped). Slight speckle remains in the armpits. |
| Bias | **0** | **Settled** | No acne on flat surfaces at 0. Very sensitive: the viewport halves z, so radius 0.2 reaches only ≈ 0.1 in depth; 0.05 (half that reach) left only deep cavities dark and everything else white. |
| Range check (ignore occluders far in front of the sample) | **Not added** | **Settled** | No halos seen at radius 0.2. |
| Blur / denoise | **Not added** | **Settled** | Armpit speckle judged acceptable. A post-blur would need pixel x/y, i.e. a real post-pass. |

---

## Modules

### `src/rasterizer/shaders/AmbientOcclusion.h/.cpp` (new)

**Responsibility:** the SSAO estimate for one point. No shader state; `MaterialShader::fragment()` calls it.

**API:**
Namespace **`AmbientOcclusion`**, following `LineDrawer` / `TriangleRasterizer` (namespace named after
the file). `Framebuffer` forward-declared in the header. **Header written (Session 61).**

- `float AmbientOcclusion::estimateAmbientOcclusion(tinymath::Vec3f position, tinymath::Vec3f normal, const std::vector<tinymath::Vec3f>& occlusion_sample_vectors, float radius, float bias, const tinymath::Matrix4x4& lookup_matrix, const Framebuffer& depth_frame_buffer)` — returns the open fraction in [0, 1]. The long name was kept over the suggested `estimate` (user's call). **Implemented (Session 62):** by-value loop variable (it is flipped, so a copy is needed anyway), open when `stored_depth <= z + bias` — the exact negation of the occluded test, so equality at bias 0 is not counted as occlusion.

The tangent-space normal is **not** extracted this lesson (see the decisions table). With the
AO-only view served by `MaterialShader`, nothing else needs it.

### `MaterialShader` (extended)

- Constructor gains: camera depth buffer, SSAO lookup transform, sample count, radius, bias. Builds
  `occlusionSampleVectors_`. **Done (Session 62):** `std::mt19937(735)` + `uniform_real_distribution<float>(-1, 1)`,
  rejection-sampled into the unit sphere (squared length ≤ 1), lengths kept — not normalised, so samples fill the
  volume and catch occluders closer than `radius`. No `reserve` (runs once). Count not stored; `size()` holds it.
- `fragment()`: calls `estimateAmbientOcclusion` with the mapped normal it already computes;
  `ambientIntensity_` is multiplied by the result.
- Doc comment fixed while there (`BlinPhong` typo; mention the normal map, shadow map and AO).

### `Application`

- `testDrawMeshMaterialShader()` — adds the camera depth pre-pass and passes the SSAO arguments.
  Checkpoint 1 is this same function with intensities `0/0/0` + ambient `1` and a 1×1 white
  diffuse texture; the end state restores the real texture and intensities.

### Tests

None planned, same position as Lessons 8–9. The candidate if a bug resists the screen:
`estimateAmbientOcclusion` against a hand-built depth buffer (a flat plane must return 1.0 with
the hemisphere and ≈0.5 if the flip is removed).

---

## Carried in from Lesson 9

- **Light direction mismatch.** The code uses `normalize({-1, 1, 1})`; the Lesson 9 doc and the
  0.8 light scale were tuned for `{1, 1, 1}`. Check that the shadow is still complete before
  blaming SSAO for any missing darkening.
- **UVs are still interpolated in screen space**, which feeds the mapped normal.
- **`vertexWorldPosition_` is still object space** — this lesson relies on that deliberately.
