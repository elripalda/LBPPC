# Precompiling shaders without building every pipeline

Research from 2026-11-04. Nothing here is implemented yet except where it says so. Measured on one PC (Ryzen 7 7800X3D, RTX 4090, driver 617.14), in one scripted run of the first level, so treat the numbers as a guide.

## The short answer

Yes, and we should not try to build everything. The game holds far too many shaders for that, but a level only touches a small share of them, and the port can see which ones as the level loads.

## What the game contains

Every `GMTb` resource (graphics material) in the game's archives holds exactly 10 PS4 pixel shaders.

| | Count |
|---|---|
| Materials in the game | 13,555 |
| Unique shaders in those materials | 135,180 |
| Shaders Ethan's cache has met so far | 888 |
| Pipelines in his cache at the time | 1,288 |

Building all of them is not practical. At the measured 29 ms per pipeline that is over an hour of CPU time for one pipeline per shader, before counting combinations.

The 10 slots are not used equally. Of the 1,147 cached pipelines that use a material shader:

| Slot | Pipelines |
|---|---|
| 2 | 555 |
| 8 | 400 |
| 0 | 133 |
| 1 | 50 |
| 3 and 9 | 9 |
| 4 to 7 | 0 |

Those 1,147 pipelines come down to 770 pixel shaders, 46 vertex shaders and 14 sets of pipeline state. Pixel shaders are nearly the whole variety.

## Where the time goes

Cold run: empty LBPPC cache and an empty NVIDIA driver cache (`__GL_SHADER_DISK_CACHE_PATH`).

| Step | Runs | Median | Total |
|---|---|---|---|
| Translating a shader to SPIR-V | 450 | 1.0 ms | 0.5 s |
| Building a pipeline in the driver | 441 | 28.6 ms | 11.6 s |

The driver is about 96% of the cost. Translating is cheap.

## Background builds already help a lot

`BB_ASYNC_PIPELINES=1` is already in the code but off by default, and the launcher doesn't set it. A new pipeline is built on worker threads and its draws are skipped until it's ready.

Same cold run with each setting:

| | Worst frames after boot | Pipeline work on the GPU thread | Lowest 5-second FPS after boot |
|---|---|---|---|
| Off (today) | 4,567, 3,004 and 2,208 ms | 11.6 s | 8 |
| On | 342 and 346 ms | 0.07 s | 27 |

The worst frames that remain line up with bursts of shader translation, which still runs on the GPU thread.

**Not checked yet:** how the skipped draws look. Objects should appear a moment late, and we need someone to watch it. We also need to confirm that skipping a draw can't change what the game simulates, since co-op depends on both PCs staying identical.

## The port can see what a level needs

The port reads the game's archives for it, so a read at a known offset identifies the resource. Tested by logging `.farc` reads and matching them against the shaders the run went on to compile:

- 476 materials were read, out of 13,555.
- All 333 material shaders the run compiled came from those materials.
- Each material was read before its shader's first use: 20 s earlier at the median, 0.2 to 0.35 s at the very least, depending on the run.

So the lead time is usually long enough to do useful work, but not always.

## Predicting whole pipelines

A pipeline also needs a vertex shader and state, which a material doesn't name. As a test, each material was left out in turn and its pipelines predicted from what other materials used at the same slot:

| Guesses per slot used | Pipelines predicted | Builds per material |
|---|---|---|
| 1 | 19% | 1.3 |
| 4 | 55% | 5 |
| 8 | 75% | 10 |
| 16 | 88% | 20 |

This test favours prediction, because it knew which slots each material really used. With 476 materials, 10 builds each is about 4,800 pipelines, or about 35 s of work on 4 threads. That's too much for 75%, so whole-pipeline prediction alone isn't the answer.

## Graphics pipeline libraries could remove the guessing

`VK_EXT_graphics_pipeline_library` builds a pipeline from separately compiled parts, then links them quickly at draw time. It suits this game's shape: 46 vertex shaders and 14 states could be prepared ahead of time, leaving only the pixel shaders, which we can identify from the archive reads.

Ethan's RTX 4090 reports `graphicsPipelineLibrary` and `graphicsPipelineLibraryFastLinking`. **Not measured:** the real link time on our shaders, support on AMD and older cards, and whether drawing with a fast-linked pipeline costs frame time.

A pixel shader's SPIR-V also depends on draw state (color formats, vertex outputs). 68 of the 1,147 pipelines needed a second version of their shader, so a prepared shader will sometimes be the wrong one and have to be built on demand.

## Recommended order

1. **Turn on background builds by default**, after watching how skipped draws look and checking co-op stays in sync. It's about one line and removes most of the freezes.
2. **Translate shaders ahead of time when a material is read.** Translate slots 2, 8, 0 and 1 on a worker thread. At about 1 ms each, 476 materials take about 2 s of CPU in total, spread over worker threads. This should remove the bursts that remain.
3. **Ship a list of known pipelines, not the shaders.** Ship shader hashes and state only, and let each player's launcher build them from their own dump on first start, so the download carries no code from the game. About 1,300 pipelines at 29 ms on 7 threads is around 5 s, an estimate that needs measuring.
4. **Prototype graphics pipeline libraries** only if pop-in is still visible after that, and measure link time first. It's the largest change here.

## What this won't fix

Noah's slow stretches in the second level showed almost no compiling, so they have another cause. Shader work only addresses hitches from new shaders.
