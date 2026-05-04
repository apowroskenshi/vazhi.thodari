#pragma once

#include "core/math_types.h"
#include <vector>
#include <string>

namespace vt {

class CameraPath {
public:
    struct Keyframe {
        float time = 0.0f;
        float x = 0, y = 0, z = 0;
        float yaw = 0, pitch = 0;
        float ibl_rot = 0.0f;
        float ibl_int = 1.0f;
        float emit_scale = 1.0f;
    };

    struct FrameParams {
        float x = 0, y = 0, z = 0;
        float yaw = 0, pitch = 0;
        float ibl_rot = 0.0f;
        float ibl_int = 1.0f;
        float emit_scale = 1.0f;
        int frame_number = 0;
        float time = 0.0f;
    };

    CameraPath() = default;

    bool load(const std::string& path);
    float duration() const;
    int totalFrames(int fps) const;
    FrameParams evaluate(int frame, int fps) const;

private:
    std::vector<Keyframe> m_keyframes;

    static float catmullRom(float p0, float p1, float p2, float p3, float t);
    static float catmullRomAngle(float a0, float a1, float a2, float a3, float t);
    static float wrapAngle(float a);
    static float lerp(float a, float b, float t);
};

} // namespace vt
