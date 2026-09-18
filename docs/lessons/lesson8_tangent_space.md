# Lesson 8 — Tangent space normal mapping

**Source:** https://haqr.eu/tinyrenderer/tangent/

> **Status: DONE (Session 57, 2026-09-18; doc written the same day).**
> `MaterialShader` gained a fourth texture read, the normal map. Each pixel solves its triangle's
> tangent and bitangent from position edges and UV deltas, and maps the decoded
> `_nm_tangent.tga` texel through `t·x + b·y + n·z`. Built under the **rung policy**: one run
> through the answer key, one deliberate break, then the end-state path. Verified by eye.

## Goal

Get each pixel's normal from a map written *relative to the surface*, so a normal map stays valid
when UV islands are reused, rotated or mirrored, which a global-space map cannot survive.

## Exit condition

- **Answer key:** the global-space `diablo3_pose_nm.tga`, decoded and used directly as the normal,
  renders with surface detail. Valid only while the model matrix is identity.
- **Deliberate break:** `_nm_tangent.tga` fed through the same global decode renders **flat**.
- **End state:** `_nm_tangent.tga` through the tangent frame renders with the same detail as the
  answer key, limbs and tail (mirrored UVs) included.

**How it was met:** the end state was judged against the expected look *by eye*. A side-by-side
A/B against the answer key was not run, by the user's decision. See *Open at the close*.

---

## Concepts

### Decoding a texel into a normal

A texel's channels are bytes in `[0, 255]`; a normal's components are in `[-1, 1]`.
`n = c / 255 · 2 − 1` per channel, then **normalize**. 8-bit quantization means the decoded vector
is almost never unit length, and `pow(N·H, shininess)` amplifies the error.

### Global space vs tangent space

A **global-space** map stores the normal itself, in object space. It works only if every texel
belongs to exactly one place on the surface. Mirrored or reused UV islands would need two
different normals in one texel.

A **tangent-space** map stores how far the normal *tilts away from the surface's own normal*, in a
local frame that moves with the surface. `(0, 0, 1)` means "no tilt". That is why these maps are
mostly blue, and why feeding one in as if it were global lights the whole model flat: every normal
is near `(0, 0, 1)` whichever way the surface faces, so `N·L ≈ 1` everywhere.

### The frame: t, b, n

- `t` (tangent) is the **3D** direction along the surface in which `u` increases.
- `b` (bitangent) is the **3D** direction in which `v` increases.
- `n` is the interpolated vertex normal.

`u` and `v` are 2D texture coordinates; `t` and `b` live in the same space as the positions and `n`.

### Solving t and b from one triangle

Worked example first. A triangle with UVs `(0,0)`, `(1,0)`, `(0,1)` gives `t = P1 − P0` and
`b = P2 − P0` directly. The UV-to-surface map is linear across a triangle, so this holds at
**every** point of it, not only near vertex 0: `t` and `b` are constant per triangle.

In general, UV edges point in arbitrary directions, so one 3D edge moves along both `u` and `v` at
once. Each edge gives one equation:

- `E₀ = P1 − P0 = Δu₀·t + Δv₀·b`
- `E₁ = P2 − P0 = Δu₁·t + Δv₁·b`

That is two equations and two unknown vectors. Solved (this is `[t b] = E · U⁻¹` written out):

- `det = Δu₀·Δv₁ − Δu₁·Δv₀`, a **scalar**: the 2D cross product of the UV deltas, twice the signed
  UV-space area.
- `t = (E₀·Δv₁ − E₁·Δv₀) / det`
- `b = (E₁·Δu₀ − E₀·Δu₁) / det`

Every `·` there is scalar × vector, not a dot product. The worked example is the case `U = I`.

### Only the determinant's sign survives

`t` and `b` are normalized, so the magnitude of `1 / det` cancels out. Its **sign** does not. It
flips `t`/`b` on mirrored UV islands, which is exactly what the limbs and tail need.

### Combining: local frame → object space

The decoded texel `(x, y, z)` is a normal in the local frame. To express it in object space,
walk `x` along `t`, `y` along `b` and `z` along `n`: `normalize(t·x + b·y + n·z)`. In Houdini
terms, `t, b, n` are the axes of a local orientation matrix, and this transforms the vector out of
it.

### Spaces must agree

Positions, `n` and the light must all be in **one** space; which one does not matter. Here all
three are object space: positions and normals come straight from the mesh, the light is a constant,
and nothing is multiplied by `transform_` (the combined matrix). With a separate model matrix,
either transform everything to world (normals by the inverse-transpose) or move the light into
object space. Mixing spaces is the only thing that breaks.

---

## Design decisions

| Decision | Choice | Reason |
|----------|--------|--------|
| Where the normal map lives | Fourth `const Texture*` (`normalMapTexture_`) in `MaterialShader` | Same texture path as diffuse/spec/glow. Not a new class. |
| Decode then normalize | Always normalize after `/255·2−1` | Quantization breaks unit length; `pow()` magnifies it. |
| `t`/`b` storage | **Locals in `fragment()`**, computed from the varyings each pixel | Caching them in `vertex()` at `vertex_index == 2` silently depends on call order 0,1,2 before any `fragment()`. Cost is one 2×2 solve per pixel. |
| `varyingNormals_` | Kept, as the frame's `n` | Not a fallback. The combine needs it. |
| Position varying name | `vertexWorldPosition_` (user's choice) | It holds **object-space** positions. That equals world only while the model matrix is identity. Revisit when a model matrix exists. |
| Division by `det` | Multiply by `one_over_determinant` | `Vec3` has no `operator/`. The reciprocal needs no new math op and no new math test. |
| `Matrix3` / 2×2 matrix type | **Not built** | The solve is two lines written out. |
| Tangent solve as a free function | **Declined**; stays inline | Proposed only after it rendered. Rule adopted: stand-alone functions are named at planning time, not extracted from working code afterwards. |
| Orthogonalizing `t`/`b` against `n` | Not done | Only matters if shading looks skewed. It did not. |
| Zero-area UV guard | None, and the count was not measured | Skipped as not relevant to the learning. |
| Global-space answer-key switch | Not kept | The shader knows only the tangent interpretation now. |
| Tests | None this lesson | The solve is inline in `fragment()` and not reachable from a test. |

---

## Modules

### `rasterizer/shaders/MaterialShader.h/.cpp` (extended)

**Responsibility:** unchanged from Lesson 7, plus the normal from a tangent-space map.

**API change:**
- Ctor gains `const Texture& normal_map_texture` after `emission_texture`.

**Members added:**
- `const Texture* normalMapTexture_`
- `std::array<tinymath::Vec3f, 3> vertexWorldPosition_`: object-space positions, filled in `vertex()`.

**`fragment()` order:** interpolate UV → sample and decode the normal map → edges and UV deltas →
`determinant`, `one_over_determinant` → normalized `t`, `b` → `interpolated_normal` →
`normalize(t·x + b·y + n·z)` → the Lesson 7 lighting, unchanged.

### `Application::testDrawMeshMaterialShader()`

Loads `models/diablo3_pose_nm_tangent.tga` and passes it in. The `.tga` still needs hand-copying
into `build/models/` on every clean build.

---

## Traps hit

- **`toVec4(vertexWorldPosition_)`** passed the whole array instead of `[vertex_index]`.
- **`delta_uv.x`/`.y` written as `[0]`/`[1]`.** `Vec2` has no `operator[]`.
- **`/ determinant` on a `Vec3`.** There is no `operator/`.
- **`tinymaht::normalize`**, a typo.
- **Confusion about the notation, not the code:** reading `E₀·Δv₁` as a dot product (it is scalar ×
  vector), and expecting the determinant to be a 2D vector (it is a scalar).

---

## Open at the close

- **Exit condition met by eye only.** If the limbs or tail ever look wrong, the first check is the
  global-vs-tangent A/B, which means temporarily restoring the global decode path.
- **Zero-area UV triangles.** `det = 0` → infinite reciprocal → NaN `t`/`b` → black or garbage
  pixels. Suspect this first for isolated black specks.
- **`t`/`b` not perpendicular to `n`.** Revisit only if the shading looks skewed.
- **`vertexWorldPosition_` is object space.** Name and space need revisiting when a separate model
  matrix arrives (likely Lesson 9).
- **UVs are still interpolated in screen space** (perspective-correct interpolation is deferred to
  last). A slightly-off edge is not automatically a tangent-basis bug.
- **`MaterialShader.h`'s doc comment** does not mention the normal map and still says `BlinPhong`.
