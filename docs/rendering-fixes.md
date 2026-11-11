# Rendering fixes: how they were found

Notes for the next graphics bug a player reports (effects or materials drawing wrongly in some level). Each entry
says what was seen, what caused it, how it was found, and where the fix is.

## Orange flares (first fix, replaced by the next section)

**Seen:** a large orange shape over the screen when collecting score bubbles, and in some cutscenes (the "Gates of
Stitchem Manor" movie on the Prologue map shows it about 10 seconds in, with no input needed). It grew over a few
seconds and stayed.

**Cause:** LBP3 clears some of its render targets with a compute shader (hash `5c5e1be6e0ada38c`) that writes zeros
over their memory as a plain buffer, binding no images. The port keeps its own copies of render targets as images, and
`run.py` sets `LBPPC_NO_BUFFER_IMAGE_INVALIDATE` because throwing those copies away on a buffer write blacks out the
3D scene. So the clear never reached the image. One target (`0x107d7a0000`, 2560x1080, drawn once a frame with
additive blending One/One and read by the final composite) was never cleared, and each frame's glow piled onto the
last.

**How it was found:**

1. A repeatable case with no input: the movie above. Frames were scored by counting orange pixels, so each change
   could be judged by a number rather than by eye (and before that, by checking the screen hadn't gone to sleep: a
   blank screen also scores low).
2. Switching off draws by vertex shader hash until the shape went: that named the shader drawing it.
3. Dumping that shader's target across frames showed it only ever got brighter: a missing clear.
4. Logging every write to that memory found the compute shader above.

**Fix:** `vk_rasterizer.cpp`. A buffer write by a shader that binds no images, over memory a render target lives in,
is remembered (`pending_target_clears`). When that target is next drawn into, the write is applied as a clear, but
only when that draw blends additively (One/One/Add): the case where a stale picture builds up. Clearing every
target the shader touched, or throwing the copies away, blacked out the scene.

**Since the root fix below** this only runs with toggle bit 61 clear and bit 62 set, and a write counts as a clear
only when it covers the whole target (a 0xe60000-byte write had cleared a 256x8192 image at `0x1088300000`).

**Log lines** (one per target, the first time): `Clear: target ... applied` for the cleared ones, `dropped` for the
rest, `dropped (blended: check this one)` for a target the write touched whose next draw blends in another way, and
`Clear warning: shader ...` for a shader that writes over a target while also binding images (not taken for a
clear). If a report shows effects building up or leaving trails, those lines say where to look first.

**If another one turns up:** look for a picture that builds up frame to frame (a missing clear like this one) or
one that is a frame behind (a buffer the game wrote that the port's image copy didn't see). The same steps work: a
place that shows it without input, draws switched off by shader, the target dumped across frames.

## Buffer writes reaching images: the root fix (0.1.0)

**Seen:** the 3D scene turning into a black, streaky mesh with the UI intact when picking up a sticker bubble
("Killer Whale Survival (YOU are the whale)": the bubble on the left at the start; also Needlepoint Peaks), on zombie
deaths in "Plants vs Zombots", and the orange flares above. Frame generation and the fix above made no difference.

**Cause:** on the PS4 images and buffers are the same memory, and LBP3 relies on it. Compute shader `5c5e1be6e0ada38c`
clears targets by writing their memory as a buffer, and scratch memory (64 MB at `0x1088300000`) is reused within a
frame as targets of different sizes. With `LBPPC_NO_BUFFER_IMAGE_INVALIDATE` those writes never reached the port's
image copies, so effect targets kept stale pictures (`0x1081ea8000`, blend src 7 / dst 6, for the pickup) and the
composite pass (vs `8957ec95`, ps `ebb656d6240b4b34`) blended them over the scene. The flare fix only guessed one case.

**How it was found:** the pickup repeated by script, frames scored by frame-to-frame jumps; draws switched off by
pixel shader while the game ran (`LBPPC_SKIP_SHADERS=<file>`, hashes one per line, re-read every 250 ms) named the
composite pass; the "Clear:" log showed its input's clear dropped.

**Fix:** `BbToggle::BufferWriteImageSync` (bit 62, on by default). Before a shader writes buffer memory, images there
holding GPU-drawn data not yet in the buffer (`NotInBuffer`) are written back to it, oldest first
(`BufferCache::WriteBackImages`); after the draw or dispatch is recorded, the images under the write reload from the
buffer before their next use (`Rasterizer::ApplyImageReloads`). No shader- or target-specific rules. Cost: about
2.4 ms of GPU time per frame at 1440p (about 166 MB of reloads); reloads skip hashing guest memory. Log:
`Image sync in the last 30 s: ...`. Bit 62 set in `user/toggles.txt` (4611686018427387904) brings back the old
behaviour live, for A/B tests.
