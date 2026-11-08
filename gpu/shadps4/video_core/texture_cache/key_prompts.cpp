// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/texture_cache/key_prompts.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

namespace VideoCore::KeyPrompts {

namespace {

// A 5x7 pixel font for the labels: each row's five pixels, leftmost in bit 4.
struct Glyph {
    char c;
    uint8_t rows[7];
};
constexpr Glyph kFont[] = {
    {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}},      {'J', {7, 2, 2, 2, 2, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 30, 1, 1, 17, 14}},
    {'6', {6, 8, 16, 30, 17, 17, 14}},   {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 2, 12}},
    {'#', {10, 10, 31, 10, 31, 10, 10}}, {'-', {0, 0, 0, 31, 0, 0, 0}},
    {'+', {0, 4, 4, 31, 4, 4, 0}},
};

const uint8_t* GlyphRows(char c) {
    for (const auto& g : kFont) {
        if (g.c == c) {
            return g.rows;
        }
    }
    return nullptr; // a space, or a character the font lacks: left empty
}

struct Rgba {
    float r, g, b, a;
};

bool InRoundedRect(float u, float v, float x0, float y0, float x1, float y1, float r) {
    if (u < x0 || u > x1 || v < y0 || v > y1) {
        return false;
    }
    const float cx = std::clamp(u, x0 + r, x1 - r), cy = std::clamp(v, y0 + r, y1 - r);
    return (u - cx) * (u - cx) + (v - cy) * (v - cy) <= r * r;
}

// The keycap in a 128x128 design space: a dark outline, a light top face and a grey front edge
// under it, the label centred on the face.
Rgba Keycap(float u, float v, const std::string& label) {
    const Rgba outline{34, 36, 44, 255}, face{238, 238, 242, 255}, front{176, 178, 190, 255},
        ink{30, 32, 40, 255}, none{34, 36, 44, 0};
    if (!InRoundedRect(u, v, 8, 12, 120, 116, 16)) {
        return none;
    }
    if (!InRoundedRect(u, v, 12, 16, 116, 112, 12)) {
        return outline;
    }
    if (!InRoundedRect(u, v, 12, 16, 116, 102, 12)) {
        return front;
    }
    const int n = int(label.size());
    if (n > 0) {
        const float units = 6.0f * n - 1.0f;
        const float scale = std::min(7.0f, 92.0f / units);
        const float x0 = 64.0f - units * scale / 2, y0 = 59.0f - 7.0f * scale / 2;
        const float gx = (u - x0) / scale, gy = (v - y0) / scale;
        if (gx >= 0 && gy >= 0 && gy < 7) {
            const int index = int(gx / 6), col = int(gx) - 6 * index, row = int(gy);
            if (index < n && col < 5) {
                const uint8_t* rows = GlyphRows(label[index]);
                if (rows && (rows[row] >> (4 - col) & 1)) {
                    return ink;
                }
            }
        }
    }
    return face;
}

// The icon sheet: one keycap over the whole texture, or (four labels) one in each quarter, top left,
// top right, bottom left, bottom right.
Rgba Sheet(float u, float v, const std::vector<std::string>& labels) {
    if (labels.size() != 4) {
        return Keycap(u, v, labels.empty() ? std::string{} : labels[0]);
    }
    const int q = (u >= 64.0f) + 2 * (v >= 64.0f);
    return Keycap(std::fmod(u, 64.0f) * 2.0f, std::fmod(v, 64.0f) * 2.0f, labels[q]);
}

// One mip of the sheet, size x size RGBA, 4x4 samples a pixel.
std::vector<uint8_t> Draw(uint32_t size, const std::vector<std::string>& labels) {
    std::vector<uint8_t> out(size_t(size) * size * 4);
    const float step = 128.0f / float(size);
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            float r = 0, g = 0, b = 0, a = 0;
            for (int sy = 0; sy < 4; ++sy) {
                for (int sx = 0; sx < 4; ++sx) {
                    const Rgba c = Sheet((x + (sx + 0.5f) / 4) * step, (y + (sy + 0.5f) / 4) * step, labels);
                    r += c.r * c.a;
                    g += c.g * c.a;
                    b += c.b * c.a;
                    a += c.a;
                }
            }
            uint8_t* p = &out[(size_t(y) * size + x) * 4];
            // Colour unweighted by coverage, so edge pixels keep the outline's colour.
            p[0] = uint8_t(a ? r / a + 0.5f : 34);
            p[1] = uint8_t(a ? g / a + 0.5f : 36);
            p[2] = uint8_t(a ? b / a + 0.5f : 44);
            p[3] = uint8_t(a / 16 + 0.5f);
        }
    }
    return out;
}

uint16_t To565(int r, int g, int b) {
    return uint16_t(((r * 31 + 127) / 255) << 11 | ((g * 63 + 127) / 255) << 5 | ((b * 31 + 127) / 255));
}
void From565(uint16_t c, int rgb[3]) {
    rgb[0] = ((c >> 11) & 31) * 255 / 31;
    rgb[1] = ((c >> 5) & 63) * 255 / 63;
    rgb[2] = (c & 31) * 255 / 31;
}

// One 4x4 block (texels clamped at the image's edge) as BC3: alpha endpoints and 3-bit indices,
// then a 4-colour BC1 block.
void EncodeBlock(const std::vector<uint8_t>& image, uint32_t size, uint32_t bx, uint32_t by, uint8_t out[16]) {
    uint8_t px[16][4];
    for (int i = 0; i < 16; ++i) {
        const uint32_t x = std::min(bx * 4 + i % 4, size - 1), y = std::min(by * 4 + i / 4, size - 1);
        std::memcpy(px[i], &image[(size_t(y) * size + x) * 4], 4);
    }
    // Alpha.
    int amax = 0, amin = 255;
    for (auto& p : px) {
        amax = std::max(amax, int(p[3]));
        amin = std::min(amin, int(p[3]));
    }
    int palette_a[8] = {amax, amin};
    for (int i = 1; i <= 6; ++i) {
        palette_a[i + 1] = (amax * (7 - i) + amin * i) / 7;
    }
    uint64_t abits = 0;
    for (int i = 0; i < 16; ++i) {
        int best = 0;
        for (int k = 1; k < 8; ++k) {
            if (std::abs(palette_a[k] - px[i][3]) < std::abs(palette_a[best] - px[i][3])) {
                best = k;
            }
        }
        abits |= uint64_t(amax == amin ? 0 : best) << (3 * i);
    }
    out[0] = uint8_t(amax);
    out[1] = uint8_t(amin);
    for (int i = 0; i < 6; ++i) {
        out[2 + i] = uint8_t(abits >> (8 * i));
    }
    // Colour: the bounding box of the visible texels.
    int lo[3] = {255, 255, 255}, hi[3] = {0, 0, 0};
    for (auto& p : px) {
        if (p[3] == 0) {
            continue;
        }
        for (int c = 0; c < 3; ++c) {
            lo[c] = std::min(lo[c], int(p[c]));
            hi[c] = std::max(hi[c], int(p[c]));
        }
    }
    if (lo[0] > hi[0]) { // all transparent
        std::copy(px[0], px[0] + 3, lo);
        std::copy(px[0], px[0] + 3, hi);
    }
    uint16_t c0 = To565(hi[0], hi[1], hi[2]), c1 = To565(lo[0], lo[1], lo[2]);
    if (c0 < c1) {
        std::swap(c0, c1);
    }
    int pal[4][3];
    From565(c0, pal[0]);
    From565(c1, pal[1]);
    for (int c = 0; c < 3; ++c) {
        pal[2][c] = (2 * pal[0][c] + pal[1][c]) / 3;
        pal[3][c] = (pal[0][c] + 2 * pal[1][c]) / 3;
    }
    uint32_t cbits = 0;
    for (int i = 0; i < 16; ++i) {
        int best = 0, best_d = 1 << 30;
        for (int k = 0; k < 4; ++k) {
            int d = 0;
            for (int c = 0; c < 3; ++c) {
                d += (pal[k][c] - px[i][c]) * (pal[k][c] - px[i][c]);
            }
            if (d < best_d) {
                best = k;
                best_d = d;
            }
        }
        cbits |= uint32_t(c0 == c1 ? 0 : best) << (2 * i);
    }
    std::memcpy(out + 8, &c0, 2);
    std::memcpy(out + 10, &c1, 2);
    std::memcpy(out + 12, &cbits, 4);
}

// The button icon sheets, by the hash of their guest data (learned with LBPPC_KEY_PROMPTS=map and
// LBPPC_KEY_DUMP_DIR): the input.ini button in each quarter, top left, top right, bottom left,
// bottom right. The shoulder buttons, sticks and Options are drawn as text, not from a sheet.
struct IconSheet {
    const char* buttons[4];
};
const std::unordered_map<uint64_t, IconSheet> kIcons = {
    {0xd09f05bebe562cacull, {{"cross", "square", "circle", "triangle"}}},
};

// LBPPC_KEY_LABELS=button=LABEL;... (run.py, from input.ini): the key shown for each button.
std::string LabelFor(const char* button) {
    const char* labels = std::getenv("LBPPC_KEY_LABELS");
    if (!labels) {
        return {};
    }
    const std::string all = labels, key = std::string(button) + "=";
    size_t at = 0;
    while (at < all.size()) {
        const size_t end = std::min(all.find(';', at), all.size());
        if (all.compare(at, key.size(), key) == 0) {
            return all.substr(at + key.size(), end - at - key.size());
        }
        at = end + 1;
    }
    return {};
}

} // namespace

bool Enabled() {
    static const bool enabled = std::getenv("LBPPC_KEY_PROMPTS") != nullptr;
    return enabled;
}

std::vector<uint8_t> Replacement(uint64_t hash, uint32_t width, uint32_t height, uint32_t mips,
                                 std::vector<uint32_t>& mip_offsets) {
    static const bool map = [] {
        const char* mode = std::getenv("LBPPC_KEY_PROMPTS");
        return mode && std::strcmp(mode, "map") == 0;
    }();
    if (width != height || width < 4) {
        return {};
    }
    std::vector<std::string> labels;
    if (map) {
        static std::mutex mutex;
        static std::unordered_map<uint64_t, int> numbers;
        std::scoped_lock lock{mutex};
        const auto [it, added] = numbers.try_emplace(hash, int(numbers.size()) + 1);
        if (added) {
            std::printf("Key prompts: icon texture %016llx is #%d\n", (unsigned long long)hash, it->second);
        }
        for (const char quarter : {'A', 'B', 'C', 'D'}) {
            labels.push_back(std::to_string(it->second) + quarter);
        }
    } else {
        const auto it = kIcons.find(hash);
        if (it == kIcons.end()) {
            return {};
        }
        bool any = false;
        for (const char* button : it->second.buttons) {
            labels.push_back(LabelFor(button));
            any |= !labels.back().empty();
        }
        if (!any) {
            return {}; // no keys bound: the controller's icons stay
        }
    }
    std::vector<uint8_t> out;
    mip_offsets.clear();
    for (uint32_t m = 0; m < mips; ++m) {
        const uint32_t size = std::max(width >> m, 1u);
        const std::vector<uint8_t> image = Draw(size, labels);
        mip_offsets.push_back(uint32_t(out.size()));
        const uint32_t blocks = (size + 3) / 4;
        for (uint32_t by = 0; by < blocks; ++by) {
            for (uint32_t bx = 0; bx < blocks; ++bx) {
                uint8_t block[16];
                EncodeBlock(image, size, bx, by, block);
                out.insert(out.end(), block, block + 16);
            }
        }
    }
    return out;
}

} // namespace VideoCore::KeyPrompts
