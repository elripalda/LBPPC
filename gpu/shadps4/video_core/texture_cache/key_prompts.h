// SPDX-License-Identifier: GPL-2.0-or-later
// lbppc: keyboard button prompts (unfinished, off: not offered in the launcher). LBP3 draws the face
// button icons from one 128x128 BC3 sheet (Cross, Square, Circle, Triangle in its quarters); with
// LBPPC_KEY_PROMPTS that sheet is replaced at upload by keycaps with the keys bound to them
// (LBPPC_KEY_LABELS, from input.ini by run.py). Missing before it can be offered: the L1/R1/L2/R2,
// stick and Options badges, which come from neither an uploaded texture nor the strings (2026-11-07:
// skipping every texture group and the text cache left them in place).
#pragma once

#include <cstdint>
#include <vector>

namespace VideoCore::KeyPrompts {

/// LBPPC_KEY_PROMPTS is set: "1" keycaps for the bound keys, "map" a number on every icon texture
/// (and its hash in the log) to learn which texture is which button.
bool Enabled();

/// The replacement for an icon texture whose guest data hashes to `hash`: BC3 blocks of every mip,
/// one mip after another (mip_offsets: where each starts). Empty when it is not a known icon.
std::vector<uint8_t> Replacement(uint64_t hash, uint32_t width, uint32_t height, uint32_t mips,
                                 std::vector<uint32_t>& mip_offsets);

} // namespace VideoCore::KeyPrompts
