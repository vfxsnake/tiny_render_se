# Phase 0.4 — Display Pipeline

**Source:** Project implementation plan (`docs/implementation_plan.md` §0.4). Engine reference: `engine/vk_tutorial_se/` (`GraphicsPipeline`, `Texture`, `Renderer`).

## Goal
Show the CPU framebuffer in the window: every frame, upload `Framebuffer::getData()` (RGBA bytes) to a GPU texture and draw a fullscreen triangle that samples it.

## Exit condition
Run the app and see the framebuffer's contents on screen, updating live. Concretely: `framebuffer_.clear(color)` (or a test gradient) fills the whole window with that image instead of the blank canvas.

## Concepts

### Vulkan bring-up
`Application` currently owns only a GLFW window. This phase stands up the rest: `VulkanContext` (instance, device, queue, surface) and `SwapChain` (the images we present to). Both already compile into the target.

### Per-frame loop (1 frame in flight)
Acquire a swapchain image → record a command buffer → submit to the queue → present, then wait on a single fence before the next frame. One command buffer, one set of sync objects (image-available semaphore, render-finished semaphore, in-flight fence). Chosen over 2+ frames in flight because the CPU rasterizer is the bottleneck this phase; the GPU work (upload + one quad) is trivial, so the fence wait is nearly free. Revisit (1→N is a localized refactor) when rasterization moves to GPU compute.

### Texture upload (persistent staging)
A host-visible **staging buffer** is allocated once and kept persistently mapped. Each frame we `memcpy` the framebuffer bytes into it, then `copyBufferToImage` into a device-local **sampled image**, wrapped in layout transitions (transfer-dst around the copy, then shader-read for sampling). No per-frame buffer allocation.

### Dynamic rendering
Vulkan 1.3 `beginRendering`/`endRendering` into the swapchain image view directly — no `VkRenderPass`/`VkFramebuffer` objects (so nothing to recreate on resize). We write the image-layout barriers by hand with `synchronization2`, matching the engine's `GraphicsPipeline::record`. No depth attachment: a single fullscreen quad has nothing to depth-test against; the CPU z-buffer is a rasterizer concern, not a display one.

### Fullscreen triangle from `SV_VertexID`
No vertex buffer: `draw(3)` and the vertex shader manufactures one oversized triangle — clip corners (-1,-1), (3,-1), (-1,3) — from the vertex index. It covers the whole screen box; the overhang is clipped by the viewport. UVs run 0→2 across the triangle, so linear interpolation lands exactly 0→1 across the visible box.

### Descriptor set (texture + sampler)
Fragment shader samples a combined image sampler at set 1, binding 0 (matches the engine's texture binding). One descriptor set layout, a small descriptor pool, one descriptor set pointing at the texture's image view + sampler.

### Format & filtering
`R8G8B8A8Unorm` — bytes map linearly to [0,1], GPU samples exactly what we wrote (no hidden gamma; color-space is a Lesson-7 topic). **Nearest** filtering — a software rasterizer must show its true pixels (staircase lines, aliasing), not a bilinear blur.

### Shader build
`shaders/display.slang` holds both entry points (`vertMain` [vertex], `fragMain` [fragment]). A CMake `add_custom_command` runs `slangc` (`-target spirv -emit-spirv-directly -fvk-use-entrypoint-name`) to compile it to `build/shaders/display.spv`, loaded at runtime via the engine's `readSpirv`. Mirrors the engine's shader toolchain; no UBO binding.

## Design decisions
| Decision | Choice | Reason |
|----------|--------|--------|
| Ownership split | Application owns `VulkanContext` + `SwapChain` + `Framebuffer`; DisplayPipeline owns command pool/buffer, sync, texture, staging, descriptors, graphics pipeline | Clean boundary: Application = bring-up/lifecycle, DisplayPipeline = "show this buffer". Avoids a god-object; no premature extra classes. |
| Frames in flight | 1 | CPU rasterizer is the bottleneck; GPU work is trivial so the fence wait is nearly free. 1→N is a localized refactor for the future GPU-compute phase. |
| Staging buffer | Allocated once, persistently mapped, `memcpy` each frame | Avoids per-frame allocation; the buffer is written every frame anyway. |
| Render path | Dynamic rendering (`beginRendering`/`endRendering`) | Matches engine; no `VkFramebuffer` to recreate on resize; manual barriers are worth learning. |
| Depth attachment | None | Single fullscreen quad — nothing to depth-test. CPU z-buffer stays a rasterizer concern. |
| Fullscreen geometry | Fullscreen triangle, `draw(3)`, positions+UVs from `SV_VertexID` | One primitive, no seam, no vertex buffer. |
| Texture format | `R8G8B8A8Unorm` | 1:1 fidelity with our `Color` bytes; no hidden gamma. |
| Sampler filter | Nearest | Show the rasterizer's true pixels; don't blur aliasing the lessons teach. |
| Descriptor binding | Combined image sampler, set 1 / binding 0 | Matches engine's texture binding convention. |
| Shader toolchain | `slangc` custom command → `build/shaders/display.spv`, loaded via `readSpirv` | Mirrors engine; single `.slang` with both entry points. |
| Verification | Visual only (no unit tests) | Display pipeline is verified by looking at the window (per CLAUDE.md testing rules). |
| Pipeline class split (Slice 3) | `GraphicsPipeline` is its own class in `src/display/`, owned by `DisplayPipeline` | One-algorithm-per-file: `GraphicsPipeline` = build+own the pipeline object; `DisplayPipeline` = orchestrate the frame. Leaner than the engine's version (no descriptor params, depth, MSAA, `Mesh`, or `record()` yet). |
| Draw-command recording (Slice 3) | Stays in `DisplayPipeline::drawFrame()`, between `beginRendering`/`endRendering`; `GraphicsPipeline` is a passive owner (no `record()`) | `DisplayPipeline` already owns the command buffer and render scope — one narrator of the frame. Promote to a `record()` only if a second pipeline ever appears. |

## Modules

### `src/display/DisplayPipeline.h/.cpp`
**Responsibility:** Own all per-frame GPU resources and turn a `Framebuffer` into on-screen pixels each frame. Non-copyable (holds Vulkan RAII handles).

**API:**
- `DisplayPipeline(VulkanContext& context, SwapChain& swap_chain, uint32_t fb_width, uint32_t fb_height)` — creates command pool + 1 command buffer; sync objects (image-available semaphore, render-finished semaphore, in-flight fence); the sampled texture (image + memory + view + sampler) at framebuffer size; the persistent mapped staging buffer; descriptor set layout/pool/set; pipeline layout + graphics pipeline (loads the SPIR-V, color format = swapchain format).
- `void drawFrame(const Framebuffer& fb)` — per-frame flow:
  1. `memcpy` `fb.getData()` → mapped staging buffer.
  2. Wait + reset the in-flight fence; acquire next swapchain image (image-available semaphore).
  3. Record the command buffer:
     - barrier texture → transfer-dst; `copyBufferToImage`; barrier texture → shader-read-only.
     - barrier swapchain image → color-attachment; `beginRendering`; bind pipeline + descriptor set; set viewport/scissor; `draw(3)`; `endRendering`; barrier swapchain image → present-src.
  4. Submit (waits image-available, signals render-finished, signals fence).
  5. Present (waits render-finished).

**Private helpers (mirror engine `Renderer`):**
- `uint32_t findMemoryType(uint32_t type_filter, vk::MemoryPropertyFlags properties) const`
- `createBuffer(size, usage, memory_properties) -> {buffer, memory}`
- `createImage(width, height, format, tiling, usage, memory_properties) -> {image, memory}`
- `createImageView(image, format, aspect) -> image_view`
- `createSampler() -> sampler` (nearest)
- `transitionImageLayout(cmd, image, old, new, srcStage, srcAccess, dstStage, dstAccess)` (synchronization2)
- `copyBufferToImage(cmd, buffer, image, width, height)`
- `createShaderModule(spirv_path) -> shader_module`
- `createGraphicsPipeline(color_format)` / `createDescriptors()`

### `src/display/GraphicsPipeline.h/.cpp` (new, Slice 3)
**Responsibility:** Build and own the fullscreen graphics pipeline. No per-frame work. Non-copyable (holds Vulkan RAII handles).

**API:**
- `GraphicsPipeline(const VulkanContext& context, vk::Format color_format)` — creates the pipeline layout and the graphics pipeline (loads the SPIR-V, dynamic rendering with `color_format`, dynamic viewport/scissor).
- `auto getPipeline() const -> const vk::raii::Pipeline&`
- (`getLayout()` added in Slice 4 when the texture descriptor arrives.)

**Private helpers:**
- `void createPipelineLayout()` — empty layout for now (no descriptors until Slice 4).
- `void createPipeline(vk::Format color_format)` — the pipeline state (`PipelineRenderingCreateInfo` for dynamic rendering, shader stages, no vertex input, triangle list, dynamic viewport+scissor, no depth, single color-blend attachment).
- `auto createShaderModule(const std::string& spirv_path) const -> vk::raii::ShaderModule`

### `src/Application.h/.cpp` (changes)
**Responsibility:** Bring-up and lifecycle; drive the frame loop.
**Changes:**
- New members: `VulkanContext context_`, `SwapChain swapChain_`, `Framebuffer framebuffer_`, `DisplayPipeline display_` (declared in construction/destruction-safe order).
- `initVulkan()` (called after `initWindow()`): construct context, swapchain, framebuffer (at window size), display pipeline.
- `mainLoop()`: each frame → (Phase-1+: rasterize into `framebuffer_`) → `display_.drawFrame(framebuffer_)`. For this phase, `framebuffer_.clear(test_color)` / gradient to prove output.
- `cleanup()`: ensure device idle before teardown; existing GLFW cleanup last.

### `shaders/display.slang`
**Responsibility:** Fullscreen-triangle passthrough that samples the uploaded texture.
- `vertMain` [vertex] — builds clip position + UV from `SV_VertexID` (0,1,2).
- `fragMain` [fragment] — `return texture_sampler.Sample(uv)` from set 1 / binding 0.

### CMake changes
- Add `src/display/DisplayPipeline.cpp` to the `TinyRendererSE` target; add `src/display` to include dirs.
- Add `slangc` `add_custom_command` producing `build/shaders/display.spv` + a `Shaders` target; `add_dependencies(TinyRendererSE Shaders)`.
- App resolves the `.spv` path relative to the build dir (mirror engine).

### Tests
None — display pipeline is verified visually (per CLAUDE.md). The exit condition is the test.
