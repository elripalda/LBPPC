// SPDX-License-Identifier: GPL-2.0-or-later
// lbppc: LittleBigPlanet 3's camera from its draws' world-view-projections (lbp_camera.h).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "video_core/renderer_vulkan/lbp_camera.h"

namespace Vulkan {

namespace {

float Dot3(const float* a, const float* b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

bool LogWanted() {
    static const bool wanted = std::getenv("LBPPC_LBP_CAMERA_LOG") != nullptr;
    return wanted;
}

/// Gram-Schmidt on the rotation rows: the synthetic view is a product of many frames' motions.
void Orthonormalize(LbpCamera::Affine& m) {
    float* x = &m[0];
    float* y = &m[4];
    float* z = &m[8];
    const auto normalize = [](float* v) {
        const float length = std::sqrt(Dot3(v, v));
        if (length > 0.0f) {
            v[0] /= length;
            v[1] /= length;
            v[2] /= length;
        }
    };
    normalize(z);
    const float yz = Dot3(y, z);
    for (int i = 0; i < 3; ++i) {
        y[i] -= yz * z[i];
    }
    normalize(y);
    const float xz = Dot3(x, z), xy = Dot3(x, y);
    for (int i = 0; i < 3; ++i) {
        x[i] -= xz * z[i] + xy * y[i];
    }
    normalize(x);
}

} // namespace

bool LbpCamera::Enabled() {
    static const bool enabled = [] {
        const char* env = std::getenv("LBPPC_LBP_CAMERA");
        return env && env[0] == '1';
    }();
    return enabled;
}

bool LbpCamera::Decompose(const float* m, Affine& vw, Proj& proj) {
    for (int i = 0; i < 16; ++i) {
        if (!std::isfinite(m[i])) {
            return false;
        }
    }
    // Row 3 gives w = view z: the camera's forward direction in object space, unit length when
    // the object is not scaled.
    const float* f = &m[12];
    if (std::fabs(std::sqrt(Dot3(f, f)) - 1.0f) > 1e-3f) {
        return false;
    }
    // Rows 0 and 1: x and y scale times the camera's right and up, plus an off-centre shift
    // along forward (zero in LBP3: no jitter).
    const float cx = Dot3(&m[0], f), cy = Dot3(&m[4], f);
    float x[3], y[3];
    for (int i = 0; i < 3; ++i) {
        x[i] = m[i] - cx * f[i];
        y[i] = m[4 + i] - cy * f[i];
    }
    const float sx = std::sqrt(Dot3(x, x)), sy = std::sqrt(Dot3(y, y));
    if (sx < 0.05f || sy < 0.05f || sx > 50.0f || sy > 50.0f || std::fabs(cx) > 0.05f * sx ||
        std::fabs(cy) > 0.05f * sy) {
        return false;
    }
    // Row 2: z scale times row 3, plus the z offset.
    const float zs = Dot3(&m[8], f);
    for (int i = 0; i < 3; ++i) {
        if (std::fabs(m[8 + i] - zs * f[i]) > 1e-3f * std::max(1.0f, std::fabs(zs))) {
            return false;
        }
    }
    if (std::fabs(Dot3(x, y)) > 1e-2f * sx * sy) {
        return false; // a sheared or non-uniformly scaled object
    }
    vw = {x[0] / sx, x[1] / sx, x[2] / sx, (m[3] - cx * m[15]) / sx,
          y[0] / sy, y[1] / sy, y[2] / sy, (m[7] - cy * m[15]) / sy,
          f[0],      f[1],      f[2],      m[15]};
    proj = {sx, sy, zs, m[11] - zs * m[15]};
    return true;
}

LbpCamera::Affine LbpCamera::Inverse(const Affine& m) {
    // General 3x3 inverse (the rows of a decomposed VW are orthonormal up to rounding).
    const float a = m[0], b = m[1], c = m[2], d = m[4], e = m[5], f = m[6], g = m[8], h = m[9],
                i = m[10];
    const float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    const float det = a * A + b * B + c * C;
    const float inv_det = det != 0.0f ? 1.0f / det : 0.0f;
    Affine r{};
    r[0] = A * inv_det;
    r[1] = -(b * i - c * h) * inv_det;
    r[2] = (b * f - c * e) * inv_det;
    r[4] = B * inv_det;
    r[5] = (a * i - c * g) * inv_det;
    r[6] = -(a * f - c * d) * inv_det;
    r[8] = C * inv_det;
    r[9] = -(a * h - b * g) * inv_det;
    r[10] = (a * e - b * d) * inv_det;
    for (int row = 0; row < 3; ++row) {
        r[row * 4 + 3] = -(r[row * 4] * m[3] + r[row * 4 + 1] * m[7] + r[row * 4 + 2] * m[11]);
    }
    return r;
}

LbpCamera::Affine LbpCamera::Multiply(const Affine& a, const Affine& b) {
    Affine r{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) {
            float v = a[row * 4] * b[col] + a[row * 4 + 1] * b[4 + col] + a[row * 4 + 2] * b[8 + col];
            if (col == 3) {
                v += a[row * 4 + 3];
            }
            r[row * 4 + col] = v;
        }
    }
    return r;
}

void LbpCamera::OnSceneDraw(u64 key, const float* wvp, VideoCore::ImageId depth) {
    std::array<float, 16> m;
    std::copy_n(wvp, 16, m.begin());
    const auto [it, added] = current.try_emplace(key, m);
    if (!added) {
        it->second[0] = NAN; // the same geometry twice in a frame (instances): ambiguous
    }
    ++scene_draws;
    if (++depth_draws[depth.index] >= 16) {
        last_depth = depth; // the last depth target with many draws: the scene's
    }
}

bool LbpCamera::EndFrame(Result& result) {
    if (!scene_draws) {
        return false;
    }
    ++frames;
    struct Cluster {
        Affine motion;
        Proj proj;
        u32 votes;
    };
    std::vector<Cluster> clusters;
    u32 pairs = 0;
    Proj any_proj{};
    bool have_proj = false;
    for (const auto& [key, m] : current) {
        Affine vw_cur;
        Proj proj_cur;
        if (!Decompose(m.data(), vw_cur, proj_cur)) {
            continue;
        }
        if (!have_proj) {
            any_proj = proj_cur;
            have_proj = true;
        }
        const auto it = previous.find(key);
        if (it == previous.end()) {
            continue;
        }
        Affine vw_prev;
        Proj proj_prev;
        if (!Decompose(it->second.data(), vw_prev, proj_prev)) {
            continue;
        }
        ++pairs;
        const Affine motion = Multiply(vw_prev, Inverse(vw_cur));
        bool joined = false;
        for (auto& cluster : clusters) {
            float rotation = 0.0f, translation = 0.0f, size = 0.0f;
            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    rotation = std::max(rotation, std::fabs(cluster.motion[row * 4 + col] -
                                                            motion[row * 4 + col]));
                }
                translation = std::max(translation, std::fabs(cluster.motion[row * 4 + 3] -
                                                              motion[row * 4 + 3]));
                size = std::max(size, std::fabs(motion[row * 4 + 3]));
            }
            if (rotation < 2e-3f && translation < 0.25f + 1e-3f * size) {
                ++cluster.votes;
                joined = true;
                break;
            }
        }
        if (!joined) {
            clusters.push_back({motion, proj_cur, 1});
        }
    }
    const auto best = std::ranges::max_element(clusters, {}, &Cluster::votes);
    const bool continuous =
        best != clusters.end() && best->votes >= 3 && best->votes * 4 >= pairs;
    if (continuous) {
        // previous view * current view^-1 = the camera motion.
        view = Multiply(Inverse(best->motion), view);
        Orthonormalize(view);
    }
    if (LogWanted()) {
        std::printf("lbppc camera: frame %llu draws %u pairs %u clusters %zu best %u%s",
                    (unsigned long long)frames, scene_draws, pairs, clusters.size(),
                    best != clusters.end() ? best->votes : 0u, continuous ? "" : " (cut)");
        if (best != clusters.end()) {
            std::printf(" move %.3f %.3f %.3f", best->motion[3], best->motion[7], best->motion[11]);
        }
        std::printf(" proj %g %g %g %g\n", any_proj[0], any_proj[1], any_proj[2], any_proj[3]);
    }
    result.view = view;
    result.inv_view = Inverse(view);
    result.proj = continuous ? best->proj : any_proj;
    result.depth = last_depth;
    result.continuous = continuous;
    previous = std::move(current);
    current.clear();
    depth_draws.clear();
    scene_draws = 0;
    const bool valid = have_proj && last_depth;
    last_depth = {};
    return valid;
}

} // namespace Vulkan
