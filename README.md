# Tiny Renderer SE

A software rasterizer built from scratch, following the [TinyRenderer](https://haqr.eu/tinyrenderer/) lecture series, displayed live inside a Vulkan window provided by the [Snake Engine](https://github.com/vfxsnake/vk_tutorial_se).

This is a learning project with two goals:

1. Implement every classic rasterization algorithm by hand — lines, triangles, z-buffering, texture mapping, lighting — entirely on the CPU, no graphics API shortcuts.
2. Bridge that CPU output into a live interactive window so the result can be seen in real time, not just written to a file.

Porting these algorithms to GPU compute shaders is **a separate project**, not a phase of this one — parallel rasterization is its own body of knowledge (SIMT execution, tiling/binning, depth-buffer atomics) and only incidentally about Vulkan. This project's CPU rasterizer is the reference implementation it will be checked against.

---

## Current status

**As of 2026-09-11 — Lesson 7 (More data!) is in progress; Lesson 6 (Shading) is complete.** The OBJ loader now reads `vt`: `Mesh` gained `textureCoordinates` and `faceTextureCoordinateIndices`, the coordinates kept as `Vec3f` so the mesh mirrors the file one-to-one — diablo's third `vt` value is ~0.6 rather than 0, so shaders keep only u and v. The next visible step is a UV-as-colour render that verifies the parse before any texture is loaded. The agreed closing plan for the rest of the project is under *Closing plan* below. Lesson 6's six shaders (`RandomShader`, `FaceShader`, `GouraudShader`, `LambertShader`, `PhongShader`, `BlinnPhongShader`) render diablo through one `drawTriangle` call, each A/B'd on screen against its predecessor, and `tests/test_shading.cpp` passes. The lesson's build log follows, oldest first. `src/rasterizer/shaders/AbstractShader.h` is written — a virtual destructor plus two pure virtuals, `vertex(face_index, vertex_index)` returning a clip-space `Vec4f` and `fragment(weights, out_color)` returning `false` to discard — and it carries **no data members**, because the varyings live on the concrete shaders. It is a `class` rather than a `struct` despite the extra `public:`, on a hazard worth knowing: **the inheritance default follows the keyword too**, so `class FaceShader : AbstractShader` would inherit *privately* and `FaceShader&` would silently fail to convert to `AbstractShader&`, with the error surfacing at the call site rather than at the declaration. The second parameter is the **corner number within the face, not a global vertex id**, because the varyings are a three-slot array — `vertex()` writes `varying_[n]` and `fragment()` reads slot 0 by `alpha`, 1 by `beta`, 2 by `gamma` — and because it is the only pair that can do the lookup, `faceIndices[face_index * 3 + n]` and `faceNormalIndices[face_index * 3 + n]` being independent (which is also why hard edges work). Writing it first required an unplanned refactor: **`screen::BarycentricWeights` lived inside `TriangleRasterizer.h`**, which is itself about to take an `AbstractShader&`, so the two headers wanted each other. Rather than forward-declare around it, the whole `screen` vocabulary was lifted into **`src/rasterizer/ScreenSpace.h/.cpp`** — `BBox`, `BarycentricWeights`, `twiceSignedArea`, `boundingBox`, `barycentricWeights`, moved verbatim — so both sides depend on the vocabulary instead of on each other; the whole namespace went rather than only the one type, because splitting a namespace across two headers is worse than either whole option. 

`drawTriangleWithShader` now exists beside the untouched `drawTriangle`, taking `const std::array<Vec4f, 3>& clip_positions`, an `AbstractShader&`, the framebuffer and a cull flag — and its **body is complete**. The shader is *forward-declared* rather than included, so `TriangleRasterizer.h` never names `shaders/` and the header cycle is closed for good. `std::array<Vec4f, 3>` rather than a `Triangle`, because `Triangle` holds `RasterVertex` (`Vec2f` + depth) and is therefore screen space by construction — the divide has happened and `w` is gone — while clip space still needs all four components. The body reuses `drawTriangle`'s skeleton verbatim (degenerate check, back-face cull, bounding box, barycentric coverage test, interpolated depth) with a prologue in front and one substitution inside: the prologue turns the three clip positions into a screen-space `Triangle`, and the constant colour is replaced by a per-pixel `shader.fragment(weights, out_color)` whose `false` return **discards** — skipping the colour *and* the depth write, so an invisible fragment cannot occlude what is behind it.

The prologue's ordering was argued rather than copied, and the argument is the reusable part. Written the obvious way — `toVec3(viewport_matrix * clip)` — it is numerically correct, because the viewport's bottom row is `(0,0,0,1)`, so `(M*v).w == v.w` and the divide still cancels the `half_width*w` translate term. It was rewritten anyway to divide **first** (`toVec3` → NDC), promote back with `toVec4` (`w` is now literally 1), then apply the viewport and read the components with no second divide. The reason is about where the divide is *attached*, not about the arithmetic: `toVec3(M * v)` divides by `(M*v).w`, so if `M` ever had a live bottom row — say `(0,0,-1/f,0)` — that becomes `-v.z/f`, a number invented from `v.z`, and the line keeps compiling while producing garbage. **The divide belongs immediately after the matrix that manufactured the `w`**; attaching it to the viewport lands on the right number only by accident of the viewport being affine. Cost of the safer order is one extra `toVec4` per vertex, register work only. Its one real forfeit: dividing first throws away each vertex's `w`, which is exactly what perspective-correct interpolation will want in Lesson 7.

The pixel loop tests depth **before** calling the shader — **early-z**, derived from the cost asymmetry: the depth test is three multiplies, an add, a buffer read and a compare, while a fragment call is open-ended (texture fetches, normalize, dots, specular pow), so only surviving pixels should pay for it. This is a *toggle* on real hardware rather than an always-on optimisation, because it is valid only while the shader cannot write depth — GPUs disable early-z the moment a shader writes `gl_FragDepth`. `AbstractShader` offers no way to do that, so this path is safe by construction.

The full space chain, with this project's own numbers read off `Projection.cpp`: model → world → view (`lookAt`) → clip (`perspective`) → **NDC** (divide by `w`; x, y in [-1, 1], z in [-1, 1]) → screen (`viewport`, which lands depth in [0, 1]). *NDC* = **N**ormalized **D**evice **C**oordinates — normalized because the ranges are fixed and unitless, *device* because it is the last space before the output device's pixel dimensions get involved, which is why nothing in it distinguishes 800×800 from 4K and why clipping belongs here rather than in pixels.

`RandomShader` is the first concrete `AbstractShader` — a constant/unlit *model* at primitive *rate*, one arbitrary colour per triangle, no lighting arithmetic anywhere. It exists to isolate the plumbing from the shading: five new mechanisms land at once with the shader path (`vertex()` called three times per face with the right indices, clip positions coming back correct, the rasterizer's new divide, its new viewport, `fragment()`'s return reaching the framebuffer), and a first shader that also lit the surface would give a wrong picture six suspects instead of five. Random rather than one constant colour, because a single colour renders the mesh as a featureless silhouette where a per-triangle geometry error hides completely — and the target image is already known, being the confetti render from Lesson 3. Colour comes from a fixed-seed `std::mt19937`: repeatable across runs, which is what makes A/B comparison possible at all. Hashing the face index was the stronger diagnostic (it binds the colour to the *face* rather than to *draw order*, so two renders stay comparable face-by-face even if traversal changes) but a shader named for randomness that contains none is misnamed, and the picture is identical either way.

Writing its includes surfaced a layering mistake: **`Mesh` lived in `src/io/ObjLoader.h`, inside `namespace io`** — the data type bundled with the thing that produces it, so a shader had to include the loader, `<string>` and `loadObj` to hold one. It now lives in **`src/geometry/Mesh.h`** in the global namespace: `io` means input/output, and a mesh is geometry that merely *arrives* that way. Deliberately **not** filed under `rasterizer/primitives/` beside `Triangle.h`, tempting as the sibling was — `Triangle` there is *screen-space raster* data, while `Mesh` is model-space geometry the Phase 2 GPU path will consume too, so filing it under `rasterizer/` would bind renderer-agnostic data to the CPU implementation. The namespace was dropped rather than renamed, matching the project's existing split: `tinymath::`, `io::` and `screen::` mark *behaviour* groupings, while the data types (`Triangle`, `Color`, `Framebuffer`) are global.

The shader's storage choices are asymmetric on purpose, and the asymmetry is the transferable part. `mesh_` is a `const Mesh*` from a `const Mesh&` parameter — the reference says "required, non-null" at the call site, the pointer keeps the member assignable. `transform_` takes a `const Matrix4x4&` parameter but is stored **by value**, because a reference member here would dangle rather than merely restrict: the call site builds `perspective * lookAt` as a temporary that dies at the end of the full expression. Sixty-four bytes copied once per shader against megabytes of mesh that genuinely outlive it. This is also the boundary of what a forward declaration can do — `struct Mesh;` suffices for a pointer, where the compiler needs only the *name*, and would not suffice for `transform_`, where it needs the *layout*; the same line divides base classes and `sizeof` from parameters and returns.

`RandomShader` now has a call site — `Application::testDrawMeshRandomShader()` — and the whole path has executed for the first time: three sessions of plumbing written blind, and the confetti render came back matching Lesson 3's on the first run, same silhouette, same framing, no seams between faces. That match is the load-bearing result, because it is what proves moving `viewport` **out** of the call site and **into** `drawTriangleWithShader` was correct. The call site composes `perspective * lookAt` only; the rasterizer owns the viewport now, so a shader never has to know the framebuffer's dimensions. The face loop fills a `std::array<Vec4f, 3>` from three separate `vertex()` calls, and **the order of those three statements is load-bearing** — the varying is written by side effect on a chosen call (`RandomShader` rolls its colour on corner 0, `FaceShader` computes its normal on corner 2, needing all three corners for two edges), so a braced initializer whose evaluation order C++ does not pin would be a real bug rather than a style choice.

`FaceShader` is **complete and verified** — flat shading, one lighting evaluation per face, and its exit condition was stricter than confetti's: reproduce `testDrawMeshMatrixLightWorldSpace()`'s grey render *pixel for pixel*, which is why the random colours were kept in a separate class rather than made a flag on this one. Run side by side with a white base colour, the two renders match on intensity and perspective, which is the load-bearing result of the whole lesson so far: it validates the programmable path end to end — order-B divide, early-z, primitive-rate varying — against a known-correct reference rather than against "it looks like a model".

It deliberately computes its normal by `cross` on the triangle's own edges rather than reading the OBJ's `vn` data, for a reason worth stating: **those file normals are per-*vertex* normals** — smoothed across adjacent faces — so using them at face rate would mean arbitrarily picking one or averaging the three, giving neither flat nor smooth shading. The cross product is the face's true geometric normal. The file normals are exactly what `GouraudShader` consumes at the next step, one per corner blended by the barycentric weights, so flat-vs-Gouraud stays a clean one-axis A/B: same light, same base colour, only the normal source and the shading rate change.

Three details of it are worth carrying forward. **`normalize` goes on the cross product, not on the two edges** — the cross of two unit vectors is unit only when they are perpendicular, and two triangle edges meet at an arbitrary angle, so normalizing the inputs instead of the output scales every face's intensity by the sine of that angle and darkens thin slivers for geometric rather than lighting reasons. **The dot needs `std::max(0.0f, ...)`** (from `<algorithm>`, not `<utility>`): a back-facing triangle yields a negative intensity, and the `uint8_t` cast on a negative float is undefined behaviour. And **the colour multiply was deliberately left in `fragment()` rather than hoisted into `vertex()`**, even though for flat shading the result is constant across the face and the hoist would cut roughly a few hundred thousand multiplies to about 2500. Not a layering argument — computing colour in the vertex stage is exactly what Gouraud shading *is* on real hardware, and `RandomShader` already does it. The argument that held is that `GouraudShader::fragment()` will be this identical body with `faceIntensity_` swapped for the weight-blended corner intensities, so keeping the multiply per pixel makes flat→Gouraud a one-line change and the per-pixel cost is cost the next shader genuinely has to pay.

`GouraudShader` is **complete and running**, and the exit condition was met on the first run: the faceting dissolves into a smooth gradient across the mesh, so the `vn` data the loader has been carrying since Lesson 3 is finally consumed. It is `FaceShader` with **one axis moved**: same four uniforms, same constructor, same light, same base colour; only the normal source and the shading rate change, which is what keeps the flat-vs-smooth A/B honest. `vertexWorldPositions_` drops out entirely, because the normal now arrives from the file rather than from the triangle's own edges, so `vertex()` needs only the corner it is currently on — a local, not a member. `faceIntensity_` becomes `std::array<float, 3> varyingIntensities_`, and that array *is* the rate change, declared in the member list. `vertex()` performs **two independent lookups from the same `face_index`/`vertex_index` pair through different index arrays** — the position via `faceIndices`, the normal via `faceNormalIndices` — and that independence is precisely what lets one point carry different normals in different faces, which is how hard edges survive smooth shading. `fragment()` is the first in the project to read its `weights` parameter: `i0·alpha + i1·beta + i2·gamma`, then the identical colour scale `FaceShader` uses. The blend needs no clamp — three non-negative intensities against non-negative weights summing to 1 cannot leave `[0, 1]`. Its call site drops the `vertices.size() != normals.size()` check that earlier ones carried: `vn` are per-corner, so their count is independent of the vertex count and that guard would bail on a valid hard-edged mesh. The `faceIndices.size() != faceNormalIndices.size()` check beside it is the real invariant, and it is load-bearing now that a shader reads `normals` at all.

**The final step was then split in two, on a one-axis-at-a-time argument.** As planned, `PhongShader` moved two axes at once — the rate to per-pixel *and* the lighting model from Lambert to `ambient + diffuse + specular` — where every prior step moved exactly one. It is now **6a: pixel rate, Lambert only** (one axis from Gouraud; the difference on this mesh is expected to be *subtle*, and that subtlety is itself the finding, since without a specular term the two shaders differ only where a triangle's normals fan sharply), followed by **6b: the reflection model**, which is where the payoff is visible and for which 6a supplies the comparison picture. Two separate classes rather than an edit in place, so 6a survives as a standing A/B rung.

6a is **`LambertShader`**, and the name was argued. Against it: `FaceShader` and `GouraudShader` are *also* Lambert, so the name states the one property all three share and none of those that distinguish it — and this lesson's own vocabulary separates Phong *shading* (a **rate**: interpolate the normal, light per pixel) from the Phong *reflection* **model** (the formula `ambient + diffuse + specular`), which would name 6a `PhongShader` and 6b `PhongReflectionShader`. The argument that won is a different axis entirely: **`FaceShader` and `GouraudShader` are teaching rungs, not materials** — each exists to demonstrate a shading rate — whereas `LambertShader` is the first *proper material*, the one that evaluates lighting where a real shader does and the one that grows a texture in Lesson 7. Naming it for the material rather than the rate marks that difference, and the existing rungs keep their names, gaining doc comments instead of renames. One correction is worth carrying into 6b, though: **Gouraud evaluating lighting in `vertex()` is not a wrong implementation** — hardware did exactly that for years. It is superseded by *what it cannot represent*, a specular highlight landing mid-triangle being evaluated at no vertex and therefore not existing at all, rather than by the lighting sitting in the wrong stage. That limitation is precisely the argument for 6b.

**6a is closed — `LambertShader` renders, and its A/B against Gouraud produced two differences worth separating.** The class is `GouraudShader` with exactly one member changed: `std::array<float, 3> varyingIntensities_` becomes `std::array<Vec3f, 3> varyingNormals_`. `vertex()` now only *carries the corner's normal through* — no lighting happens in the vertex stage any more, which is the entirety of the axis being moved — and `fragment()` blends the three normals with the weights, re-normalizes, and evaluates `max(0, dot(n, l))` at the pixel.

Two bugs and one deliberate omission made the step. The omission first: `fragment()` was written **without** the re-normalize on purpose, because a barycentric blend of three unit vectors lands *inside* the unit sphere, and the prediction was that the resulting sag would darken triangle interiors. It did — the render came back uniformly darker than Gouraud, and adding `normalize` lifted it. Three characters, watched rather than argued. The bug that preceded it was `std::min` where `std::max` belonged, and its symptom is worth recording because it read as something else entirely: with `min(0, d)` every intensity is ≤ 0, so surfaces *facing* the light clamp to `0` and go black while surfaces facing *away* get a **negative** intensity, and a negative float cast to `uint8_t` is undefined behaviour that in practice wraps to arbitrary bright values. Lit and unlit regions swapped, which presents as "the light is coming from behind" — and flipping the mesh normals appears to half-fix it, because that only swaps which set of faces gets the garbage branch and which gets the zero, leaving the zeroed faces as black holes.

**The second difference is not a bug and survives the fix:** Lambert's shadow terminator is larger than Gouraud's. Gouraud clamps `max(0, ·)` **per corner and then blends**, so a triangle straddling the terminator blends a clamped `0` against a positive value and stays lit — it leaks light past the boundary. Lambert blends the raw normals and clamps **once, at the end**, so the blended normal genuinely points away from the light there and the pixel goes dark. Lambert's terminator is the correct one; Gouraud's is smeared. On this mesh that, plus a faint sharpening where a triangle's normals fan, is the whole visible delta — exactly the predicted subtlety, and the reason 6b exists.

All five shaders now carry **class doc comments** stating each one's rate, its purpose, and what it cannot represent — including which of them impose an **ordering contract** on the three `vertex()` calls (`RandomShader` rolls its colour on corner 0, `FaceShader` needs all three corners by corner 2; `GouraudShader` and `LambertShader` write only their own slot and so have none).

`LambertShader.h` is written and reviewed — `GouraudShader.h` with exactly one member changed, `std::array<float, 3> varyingIntensities_` becoming `std::array<tinymath::Vec3f, 3> varyingNormals_`; `= {}` suffices to zero it because `Vec3` is an aggregate with no user-provided constructor, so value-initialization reaches all three components. The `.cpp` is still a copy of Gouraud's and does not yet compile. What remains is the two body changes: `vertex()` storing the raw normal rather than the dot, and `fragment()` blending the three normals, **re-normalizing**, then evaluating `max(0, n·l)`. That re-normalize is 6a's *show it* — a barycentric blend of three unit vectors lands *inside* the sphere rather than on it, so leaving it out sags the intensity toward triangle interiors; it is three characters to break and put back.

Two things about it are worth carrying forward. **The side-effect-on-a-chosen-call contract disappears here.** `RandomShader` rolls its colour on corner 0 and `FaceShader` computes its normal on corner 2 (two edges need all three corners), so for both of them the three `vertex()` statements at the call site are order-dependent with nothing in the type system saying so. `GouraudShader` writes `varyingIntensities_[vertex_index]` and touches nothing else, so its corners are independent and may be evaluated in any order — the first shader in the series with no unenforced ordering contract. And the call sites carry a check that is **not actually an invariant**: `vertices.size() != normals.size()`. The `vn` are per-corner, so their count is independent of the vertex count — it holds for `diablo3_pose.obj` and would bail on a valid mesh with hard edges. The `faceIndices.size() != faceNormalIndices.size()` check beside it is the real one, and it is finally load-bearing now that a shader reads `normals` at all.

**Step 6b is closed — `PhongShader` renders the Phong reflection model, and the exit condition was met on both halves: the specular highlight is visible under Phong and **absent** under Gouraud.** With `shininess = 200` the highlight is small enough to land *between* a triangle's corners, and a vertex-rate shader evaluates lighting at no point inside that region — so the highlight does not merely blur, it does not exist. That absence, not the highlight itself, is the argument for per-pixel lighting. The shader keeps 6a's varyings unchanged (`std::array<Vec3f, 3> varyingNormals_` — 6b moves the **model** axis only) and adds `viewDirection_`, `specularColor_`, `ambient_`, `shininess_`.

The reflection vector is written as **four named geometric steps rather than the one-liner**, because the formula is worth watching assemble. `L_proj = (N·L) * N` is the part of the light vector lying along the normal — the dot product measures *how much* of `L` points along `N`, and multiplying that scalar back onto `N` rebuilds it as a vector. `L_flat = L - L_proj` is the remainder, the component in the surface plane; "flat" because with `N` as up it has no height. `R = L_proj - L_flat` keeps the normal-aligned part and **negates the tangential one**, and that single sign flip *is* the reflection — mirroring across a surface leaves the up part alone and reverses the sideways part. Substituting back gives `2*L_proj - L`, so `2(N·L)N - L` falls out instead of being asserted. `R` is deliberately **not** normalized: N and L are unit and reflection is an isometry, so it comes out unit for free — a `normalize` there is a `sqrt` and three divides per pixel for nothing.

**Why the exponent, which is the concept of the step.** A cosine decays extremely slowly near its peak — 10° off the mirror direction is still 0.985, 30° off is still 0.87 — so `R·V` used raw would paint a soft glow over the entire lit hemisphere, visually indistinguishable from the diffuse term already there. Raising it to a power squashes everything except values very close to 1 (at exponent 100 that 10°-off point falls to ≈0.22) while the peak stays at exactly 1, because 1 to any power is 1. **The highlight keeps its brightness and loses its width.** `shininess` is therefore an angular-width knob — low is a broad dull sheen, high a tight polished dot — and the width shrinks roughly like 1/√n, which is why 10 → 20 is visible and 200 → 210 is not.

**None of this is physics.** Phong's 1975 paper presents `cos^n` as empirical: it looks like a highlight and costs one instruction. It has no **energy conservation** (raising `shininess` shrinks the highlight without brightening it, so the surface reflects less total light — physically-based versions bolt on a `(n+2)/2π` normalization), no **Fresnel** (every real dielectric goes near-mirror at grazing angles, which is why a matte tabletop reflects when you crouch), and no **shadowing/masking** of microscopic surface detail. The physical lineage runs Torrance–Sparrow → Cook–Torrance → GGX, all deriving the lobe from the statistical normal distribution of millions of tiny mirrors. The footnote that makes the story: Blinn-Phong's `(N·H)^n` turns out to be a close fit to the Beckmann microfacet distribution — the convenient hack landed near the real answer, which is why it survived thirty years.

Two decisions in the shader are approximations taken knowingly. **The view direction is the infinite-viewer approximation** — the call site passes a *constant* `normalize(eye)` where the true per-pixel value is `normalize(eye - world_position)`. It is correct only because the model sits at the origin, and the error grows toward the silhouette; the shader cannot do better as written, since `vertex()` discards the world position after transforming, so the exact version costs a `varyingWorldPositions_` member. This is what fixed-function hardware did. And **specular is evaluated even where `N·L < 0`**, so a highlight can in principle appear on a face turned away from the light; looked for on screen at `shininess = 200` and not visible on diablo, so the gate stays parked rather than fixed. The review that got there is worth one line of its own: the load-bearing bug was `dot(R, L)` where `dot(R, V)` belonged — `R·L` expands to `2(N·L)² - 1`, which contains **no view term**, so the highlight would be welded to the surface and would not move when the camera orbits. The rule that settled it: everything upstream of `R` is about the light, and `viewDirection_` appears exactly once in the whole function, because specular is the only term that reads the camera.

**Step 6c closed the lesson — `BlinnPhongShader` renders, and both registered predictions were confirmed on screen.** It is `PhongShader` with only the specular term changed: `R·V` becomes `N·H`, with `H = normalize(L + V)` — the diagonal of the rhombus spanned by two unit vectors, so it bisects the angle between light and eye; physically, the normal a perfect mirror would need to bounce the light straight into the eye. `H` is computed in `fragment()` rather than hoisted to the constructor, by choice, so the whole model reads in one place (and because it has to live there once the view direction goes per-pixel). **Prediction 1:** at equal `shininess` the Blinn highlight is wider — for coplanar vectors the `N`–`H` angle is exactly half the `R`–`V` angle, and lobe width goes like 1/√n, so Blinn at **800** matched Phong at 200. **Prediction 2:** with the light behind the model at the camera's elevation (grazing on upward-facing surfaces), Blinn's highlight stretches into a streak while Phong's stays round, and the streak is the physically correct one. The unlit-side specular artifact still did not appear — at these exponents the best reachable value on a face turned from the light is ≈ e⁻³² — so the `N·L > 0` gate stays parked.

At the close, `drawTriangleWithShader` was renamed **`drawTriangle`** and the constant-colour path became **`drawTriangleSolidColor`** (not *Flat*, which names a shading rate in this project's vocabulary), both with doc comments. **`tests/test_shading.cpp` passes**: exact corner values and the centroid mean of a varying; `FaceShader` against a hand-computed `max(0, n·l)` with the file normals deliberately pointing away; Lambert's re-normalize; full specular for both models at the mirror orientation; the half-angle relation; and discard leaving colour *and* depth untouched, asserted in the strong form where a farther triangle drawn after a discarded nearer one must still win. The five deferred `ObjLoader` findings moved to Lesson 7, which rewrites the same parser to read `vt`.

The lesson kick-off protocol changed at the start of this lesson: **spike first, document second.** Concepts are triaged into *show it* (has a visible failure mode — spike and break it, don't pre-discuss), *measure it* (answerable by running numbers over the real mesh) and *discuss it* (genuinely invisible — design forks and effects that only manifest later). The plan doc now records the design the spike earned rather than blocking the first pixels.

Overall arc: work through the TinyRenderer lessons in order (CPU rasterizer, by hand) until an OBJ model renders textured and shaded in the live window. **The GPU compute port was split out into a separate project** rather than kept as a Phase 2 — its concepts are general parallelization (SIMT divergence, tiling vs per-pixel work assignment, atomic depth writes) with only a thin Vulkan shell, so bundling it here would have hidden a whole second subject inside this one's scope. Design decisions taken here *for* GPU portability still stand and are recorded in the lesson docs; only the packaging changed.

**Closing plan (agreed 2026-09-11) — build for the end result, fewer rungs, dive deeper only on request.** The end result is diablo with diffuse, tangent-space normal, specular and glow maps, lit by Blinn-Phong and shadowed, with ambient occlusion and toon shading as closing extras. Five runs remain, in order:

1. **Textured** — `vt` in `Mesh`/`ObjLoader` (done), texture loading through `stb_image` (already fetched by CMake), nearest sampling, and **one end-state shader** taking diffuse, spec map and glow at once, since they are three reads of the same texture code.
2. **Tangent-space normal map** — basis from `[t, b] = E · U⁻¹` (a 2×2 inverse), applied as `t·x + b·y + n·z` with no `Matrix3` type. Validated by matching a one-off render of the global-space map `_nm.tga`, which is a check rather than a rung.
3. **Shadow mapping** — depth-only pass from the light, depth compare, acne fixed with a bias. The lookup goes through a **world-position varying** rather than the lesson's `N M⁻¹`, so no general 4×4 inverse is needed, and the exact per-pixel view direction comes with it.
4. **Ambient occlusion** — baked from many shadow maps and/or screen-space from depth + normal buffers.
5. **Toon** — quantized intensity plus a Sobel edge pass over the z-buffer, the project's first post-process pass.

**Perspective-correct interpolation is optional and last.** It is not on the lesson pages — this project added it — and it lives entirely in `drawTriangle` (keep `w` through the prologue, correct the weights), so no shader changes. Deferring it means every varying, UVs and world positions included, stays screen-space interpolated until then: small on diablo's small triangles, but a slightly-off texture or shadow edge before that point should be suspected of this before the tangent basis or the bias. Loader input validation was dropped by decision — the models are known-good and the project is not used outside them.

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
| 6 | Shading | Complete |
| 7 | More data! (textures, normal & specular maps) | In progress |
| 8 | Tangent space | Planned |
| 9 | Shadow mapping | Planned |
| 10 | Ambient occlusion | Planned |
| 11 | Toon shading (bonus) | Planned |
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

