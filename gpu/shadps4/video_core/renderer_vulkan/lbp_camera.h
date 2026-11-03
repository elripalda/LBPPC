// SPDX-License-Identifier: GPL-2.0-or-later
// lbppc: the camera of LittleBigPlanet 3's frames, for frame generation (CameraMotion). LBP3 has
// no camera constants of its own: each draw gets its world-view-projection (WVP) in rows at the
// start of a vertex shader buffer. A WVP is split into the projection and the object's
// world-to-view transform VW; for an object that did not move, VW_prev * VW_cur^-1 is the
// camera's motion, the same for all of them. The motion most draws agree on is the camera's.
// LBPPC_LBP_CAMERA=1 turns it on (run.py, for LBP3); LBPPC_LBP_CAMERA_LOG=1 logs each frame.

#pragma once

#include <array>
#include <unordered_map>

#include "common/types.h"
#include "video_core/texture_cache/types.h"

namespace Vulkan {

class LbpCamera {
public:
    using Affine = std::array<float, 12>; ///< 3x4 rows
    using Proj = std::array<float, 4>;    ///< x scale, y scale, z scale, z offset (w = view z)

    static bool Enabled();

    /// A draw into a depth target whose vertex shader buffer starts with `wvp` (16 floats,
    /// rows). `key` identifies the draw's geometry across frames.
    void OnSceneDraw(u64 key, const float* wvp, VideoCore::ImageId depth);

    /// Whether a scene draw was seen since the last EndFrame (the next display pass ends it).
    [[nodiscard]] bool HasScene() const noexcept {
        return scene_draws != 0;
    }

    struct Result {
        Affine view{};          ///< world (synthetic, continuous) to view
        Affine inv_view{};
        Proj proj{};
        VideoCore::ImageId depth{};
        bool continuous = false; ///< the camera moved from the previous frame's (motion valid)
    };
    /// At the frame's display pass: the frame's camera, or false without scene draws.
    bool EndFrame(Result& result);

    /// Splits a WVP; false when it is not a perspective view of an unscaled object.
    static bool Decompose(const float* wvp, Affine& vw, Proj& proj);
    static Affine Inverse(const Affine& m);
    static Affine Multiply(const Affine& a, const Affine& b);

private:
    std::unordered_map<u64, std::array<float, 16>> current, previous;
    std::unordered_map<u32, u32> depth_draws; ///< ImageId index -> draws this frame
    VideoCore::ImageId last_depth{};
    u32 scene_draws = 0;
    Affine view{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    u64 frames = 0;
};

} // namespace Vulkan
