# Phase 0.3 — Timer Utility

**Source:** Infrastructure (not a TinyRenderer lesson) — `docs/implementation_plan.md` §0.3

## Goal
A header-only stopwatch to benchmark rasterizer iterations (naive vs. optimised line drawing, triangle fill strategies, etc.).

## Exit condition
A block of work wrapped in `start()` / `stop()` reports a plausible elapsed time in ms and µs. No unit tests — correctness is judged by sensible output values.

## Concepts

### Monotonic vs. wall-clock time
`system_clock` tracks calendar time and can jump (NTP sync, DST) — unsafe for measuring durations. `steady_clock` is guaranteed monotonic and constant-rate; its zero is arbitrary but `end - start` is always a valid non-negative duration. `high_resolution_clock` may alias either, so it is not portable for interval timing. → Use `steady_clock`.

### Derived vs. stored state
The elapsed interval is fully determined by the two captured time points, so it is not stored — storing it would be redundant state that can go stale, and fixing it to one unit (ms) would lose precision needed by `elapsedUs()`. Store the raw `time_point`s; convert on demand.

### chrono duration conversion
`end_ - start_` yields a `std::chrono::duration`. Converting with `std::chrono::duration<double, std::milli>(d).count()` gives floating-point milliseconds (fractional part preserved), unlike `duration_cast<milliseconds>` which truncates to integer.

## Design decisions
| Decision | Choice | Reason |
|----------|--------|--------|
| Clock | `std::chrono::steady_clock` | Monotonic; never jumps — correct for durations |
| State | two `time_point` members `start_`, `end_` | Interval is derivable; raw points keep full precision |
| Return type | `double` | Keeps fractional ms/µs — meaningful for benchmarking |
| Conversion | `std::chrono::duration<double, std::milli / std::micro>` | Float units, no truncation |
| Misuse policy | Trust the caller; document "call `stop()` before reading" | Simplest; always used in tight start/stop pairs |
| `ScopedTimer` | Deferred to Lesson 1 | RAII "one-line" timer; composes on `Timer`, add when first needed |

## Modules

### `src/utils/Timer.h` (header-only)
**Responsibility:** Capture two `steady_clock` time points and report the interval between them in ms/µs.
**API:**
- `void start()` — capture `start_ = steady_clock::now()`
- `void stop()` — capture `end_ = steady_clock::now()`
- `double elapsedMs() const` — `duration<double, std::milli>(end_ - start_).count()`
- `double elapsedUs() const` — `duration<double, std::micro>(end_ - start_).count()`

**Contract:** call `start()` then `stop()` before reading an `elapsed*()`. Reading without a prior `stop()` returns an undefined/stale value (members default-initialise to the clock epoch, so a fresh unused Timer reads ~0).

**No `.cpp`, no tests** — header-only per the math/util convention; correctness verified by plausible output.

## Follow-on (not part of 0.3)

### `ScopedTimer` (Lesson 1, when first benchmarking)
RAII wrapper: constructor `start()`s, destructor `stop()`s and prints. Gives a one-line, easily-removable timer around any scope. Composes on top of `Timer` without modifying it.
