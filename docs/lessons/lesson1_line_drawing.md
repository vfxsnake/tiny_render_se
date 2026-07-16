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

### What actually justifies Bresenham (measured finding, Session 17)
The textbook motivation for leaving floats behind is "accumulated error drifts the line." Worked
through honestly, **that argument does not hold at screen scale**, and it is worth recording why:

- `minor_axis_step * i` (rung 1) performs *one* multiply and *one* rounding per pixel — the error
  stays proportional (~`1e-7` relative) and never compounds.
- Rung 2 (`y_acc += minor_axis_step`) *does* compound: `1/3` in `float` is high by ~`1e-8`, so an
  800-px line accumulates ~`8e-6` of bias plus a random walk of ~`4e-4`. Total: ~1/1000 of a pixel.
- `round` only changes its answer when a value crosses `.5`. For slope `1/3` the accumulator's
  fractional part is only ever `0`, `0.333`, or `0.667` — never closer than `1/6` (~`0.167`) to a
  boundary. Drift would need to be **~400× larger** to move a single pixel.

So the real case for the integer rung is:
1. **Speed** — integer add + compare vs. float add + `round` + int conversion. Measurable; benchmark it.
2. **Determinism** — bit-identical output across compilers, platforms, and optimization levels.
   This matters directly to Phase 2: the CPU rasterizer is the *reference implementation* for the
   GPU version, and "do CPU and GPU agree?" is only answerable if the CPU answer is exact.
3. **Drift** — real in principle, unobservable here. Understand it; don't claim to see it.

Note the tension worth remembering: the speed fix (multiply → accumulate) is what *creates* the
drift problem. Rung 2 is faster and less exact than rung 1. Bresenham is the rung that is both.

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
| `LineDrawer` shape | **Namespace with free functions** (supersedes "static method on a class") | No state, so no class |
| Steepness handling | **Transpose** (not branch-on-axis) | Single hot-loop path; error logic (rungs 4–5) lives in one place |
| Direction handling | Swap endpoints so lead axis ascends | Fixed anchor for error accumulation; `draw(A,B)==draw(B,A)` |
| Swap vs. `direction` flag | **Swap only** — no `direction` multiplier | After the swap the loop always marches `+1`; kills a per-pixel branch. The rejected hybrid swapped, then swapped *back* in-loop with `direction=-1` — the two cancelled |
| Delta measurement | **Measure twice**: before normalization (steep test) and after (step calc) | Different questions. Pre-transpose deltas answer "is it steep?"; post-swap deltas carry the correct sign. Reusing the pre-swap deltas after a swap is a sign bug |
| Endpoint contract | **Inclusive of both endpoints** (`i <= delta_x`) | Matches TinyRenderer; closed shapes need it — an exclusive end leaves a 1-px hole at every corner of a triangle |
| Rung coexistence | Three named functions side by side: `drawLineNaive` / `drawLineAccum` / `drawLineBresenham` | All three must exist at once to be benchmarked against each other; the losers are deleted after the numbers are recorded |
| Shared code across rungs | Normalization only (file-local helper, anonymous namespace) — **never the inner loop** | Normalization runs once per line (free). A function pointer in the *per-pixel* loop blocks inlining and its overhead would swamp the float-vs-int difference under measurement |
| Benchmark location | `bench/bench_linedrawer.cpp`, standalone `main()` — not in `LineDrawer`, not a Catch2 test | The thing measured must not contain the measuring: keeps `<random>`/`<iostream>`/`Timer` out of the rasterizer and out of the app binary. No pass/fail, so not CTest |
| Rung selection at bench time | Function pointer (`using LineFn = void(*)(Vec2i, Vec2i, Color, Framebuffer&)`) | Identical signatures; harness constant, algorithm variable. One indirect call *per line* is amortized over hundreds of `setPixel`s |
| Zero-length line (`a == b`) | **Open** — precondition or guard, undecided | `delta_x == 0` → `0.0f/0.0f` = NaN → `int` conversion is UB. Needs a contract decision, not a patch |
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
- `void drawLineNaive(Vec2i a, Vec2i b, Color color, Framebuffer& fb)` — rung 1. Float
  `minor_axis_step * i` + `std::round` per pixel. Correct, all 8 octants. **Done.**
- `void drawLineAccum(Vec2i a, Vec2i b, Color color, Framebuffer& fb)` — rung 2. Hoists the
  multiply into a `float y_acc += minor_axis_step`. Keeps `std::round` (dropping it for
  `static_cast` would change *which* pixels light up — truncate-toward-zero vs. nearest — and
  conflate two changes). _(to write)_
- `void drawLineBresenham(Vec2i a, Vec2i b, Color color, Framebuffer& fb)` — rung 3. Integer
  error term scaled by `delta_x`; add + compare only. _(to write)_

**File-local (anonymous namespace):**
- `bool normalize(Vec2i& a, Vec2i& b)` — steep test → transpose → endpoint swap; returns whether
  transposed. Shared by all three rungs; caller recomputes deltas afterward.

(A stop-count parameter is expected to emerge for the animated reveal.)

### `bench/bench_linedrawer.cpp`
**Responsibility:** time the rungs against each other. Standalone `main()`; links
`LineDrawer.cpp` + `Framebuffer.cpp` only (no Vulkan/GLFW/Catch2). First consumer of `Timer` —
closes the Phase 0.3 exit condition, open since Session 4.
**Method:**
- Fixed-seed RNG, lines **pre-generated into a vector before `timer.start()`** (RNG cost is
  identical across rungs and would only dilute the difference).
- All rungs draw the *same* lines.
- Build with optimizations or the numbers are meaningless — the root CMakeLists never sets
  `CMAKE_BUILD_TYPE`, so `build_wsl` is `-O0`:
  `cmake -S . -B build_release -G Ninja -DCMAKE_BUILD_TYPE=Release`
- Print a byte from `fb.getData()` after the timer stops, so the writes stay observable and the
  optimizer can't legally delete the timed loop. (Cheap insurance; a cross-TU call probably
  prevents this anyway, but a `0 ms` result is a confusing thing to debug.)
- Random lines make warm-up and min-of-N refinements unnecessary — fixed costs amortize across
  ~1M draws.
**Not** benchmarked through the app's main loop: that would time the memcpy, upload, barriers,
and a vsync-blocked present — i.e. mostly the monitor's refresh rate.

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
