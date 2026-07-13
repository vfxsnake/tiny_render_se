# Lesson 1 — Bresenham's Line Drawing

**Source:** https://haqr.eu/tinyrenderer/bresenham/

> This doc is a **living record of a discovery-driven lesson**. It captures the design as
> it is discovered on screen, not a full spec fixed up front. Parts marked _(to emerge)_
> are deliberately deferred until the visible goal demands them.

## Goal
Draw a gap-free straight line between two 2D points by setting individual pixels — the
primitive wireframe rendering is built on.

## Exit condition
`LineDrawer` draws gap-free lines at **any angle and any direction** into `framebuffer_`,
visible in the window (test lines / a simple wireframe replacing `drawTestPattern()`),
backed by a unit test on pixel coverage. Bonus: the line can be watched drawing over time
via a delta-time reveal.

## The discovery chain
The visible goal pulls each primitive into existence, in this order:

1. **Draw a point** → need a way to name a position → `Vec2<int>`.
   (`Framebuffer::setPixel` + `Color` already exist — the draw-point is already ours.)
2. **Draw many points along a line** → need something that decides *which* pixels → `LineDrawer::drawLine`.
3. **Watch it draw over time** → need a delta-time game loop in `Application`; the reveal
   count per frame surfaces a stop-parameter on `drawLine` _(to emerge)_.
4. **Measure it** → `Timer` (Phase 0.3) gets its first consumer — both dt source and benchmark.
5. **Make it correct & fast** → the naive→Bresenham rungs _(to emerge as we iterate)_.

## Concepts

### The Framebuffer is the boundary
The rasterizer's only job, ever: compute a **color** (and later a **depth**) for a pixel and
write it via `setPixel` / `setDepth`. Every TinyRenderer feature bottoms out in those two
calls. The display side is frozen — it re-uploads `getData()` every frame and never changes
again for CPU rasterization. This lesson touches only `setPixel`; `setDepth` waits for Lesson 4.

### Why `Vec2`
A position is *one thing*, not two loose ints. `Vec2` moves the pair as a unit and later
becomes the home for vector algebra (`+`, `-`, scale, `dot`, `cross`). Those operations are
**not built now** — only `x`/`y` — and arrive when a lesson needs them (dot/cross → barycentric
& lighting). Templated so `Vec2<int>` (pixels now) and `Vec2<float>` (later) share one type.

### Line-drawing progression (naive → Bresenham)
1. **Parametric `t` stepping** — `P(t)=A+t(B−A)`, fixed step. Flaw: fixed step is divorced from
   line length → gaps (too few samples) or overdraw.
2. **Sweep the lead axis** — loop the integer axis, derive the other. Flaw: hard-coding x as the
   lead axis gaps out steep lines.
3. **Transpose (or branch on dominant axis)** — iterate the axis of largest change; normalize
   direction so `draw(A,B)==draw(B,A)`. Two implementations of the same idea (see decisions).
4. **Incremental error (float)** — y changes by a constant amount per x-step → accumulate error,
   bump y by ±1 past the half-pixel threshold; kills the per-pixel division.
5. **Integer Bresenham** — scale error by `2·(bx−ax)` → all-integer inner loop, no floats.

### Delta-time reveal (proper game-loop pattern)
`mainLoop` becomes update-with-dt then render: `Timer` gives `dt`, `elapsed += dt`, reveal =
`rate * elapsed`. Reveal speed is resolution-independent by construction — **no sleep** needed
for speed control. An explicit frame-cap wait is a *separate*, optional lever (present is likely
vsync-capped already). `drawLine` stays count-based and clock-ignorant; the loop owns time. This
interleaving (one slice of the algorithm per presented frame) is what makes the draw *visible* —
the display only samples the framebuffer once per `drawFrame`.

## Design decisions
| Decision | Choice | Reason |
|----------|--------|--------|
| Position type | `Vec2<T>` templated, `x`/`y` only | One value for a position; int now, float later; no premature ops |
| Vec2 operators | None yet | No on-screen consumer for dot/cross until later lessons |
| `LineDrawer` shape | Free `static` method, framebuffer passed in | Stateless; matches "Framebuffer is the boundary"; easy to test |
| Steepness handling | **Transpose** (not branch-on-axis) | Single hot-loop path; error logic (rungs 4–5) lives in one place |
| Direction handling | Swap endpoints so lead axis ascends | Fixed anchor for error accumulation; `draw(A,B)==draw(B,A)` |
| Animation control | Delta-time in `Application`, not in `drawLine` | dt lives with the loop; `drawLine` stays pure/benchmarkable |
| Reveal speed | `rate * elapsed` (dt-scaled), no sleep | Frame-rate independent; sleep only for optional FPS cap |
| Slice parameter | _(to emerge)_ | Let the animation goal surface the need for a stop-count |

## Modules

### `src/math/Vec2.h`
**Responsibility:** name a 2D position/vector as one value.
**API (this lesson):**
- `template<class T> struct Vec2 { T x, y; };` — members only, header-only.

### `src/rasterizer/LineDrawer.h/.cpp`
**Responsibility:** decide which pixels lie between two endpoints and set them.
**API:**
- `static void drawLine(Vec2<int> a, Vec2<int> b, Color color, Framebuffer& fb)` — draw a
  gap-free line. (A stop-count parameter is expected to emerge for the animated reveal.)

### `src/Application` (changes)
- Delta-time game loop; `Timer` supplies `dt`; producer draws lines into `framebuffer_`.
- `drawTestPattern()` retired once real lines render.

### `tests/test_vec2.cpp`
**Cases:**
- construct & read back `x`/`y` for `Vec2<int>` and `Vec2<float>`.

### `tests/test_linedrawer.cpp`
**Cases (to firm up):**
- horizontal, vertical, 45° lines hit expected pixels;
- steep line has no gaps (every lead-axis step sets a pixel);
- `draw(A,B)` and `draw(B,A)` produce identical pixel sets;
- endpoints are set.
