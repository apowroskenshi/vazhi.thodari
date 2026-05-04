#include "camera/camera_path.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace vt {

bool CameraPath::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        fprintf(stderr, "Failed to open camera path: %s\n", path.c_str());
        return false;
    }

    m_keyframes.clear();
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        Keyframe kf;
        if (!(iss >> kf.time >> kf.x >> kf.y >> kf.z >> kf.yaw >> kf.pitch))
            continue;
        iss >> kf.ibl_rot >> kf.ibl_int >> kf.emit_scale;

        m_keyframes.push_back(kf);
    }

    std::sort(m_keyframes.begin(), m_keyframes.end(),
              [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });

    printf("Camera path loaded: %zu keyframes, %.1f seconds\n",
           m_keyframes.size(), duration());
    return true;
}

float CameraPath::duration() const {
    if (m_keyframes.size() < 2) return 0.0f;
    return m_keyframes.back().time - m_keyframes.front().time;
}

int CameraPath::totalFrames(int fps) const {
    return static_cast<int>(std::ceil(duration() * fps)) + 1;
}

CameraPath::FrameParams CameraPath::evaluate(int frame, int fps) const {
    FrameParams fp = {};
    fp.frame_number = frame;
    if (m_keyframes.empty()) return fp;

    float t = m_keyframes.front().time + static_cast<float>(frame) / fps;
    fp.time = t;

    auto fill = [](FrameParams& fp, const Keyframe& kf) {
        fp.x = kf.x; fp.y = kf.y; fp.z = kf.z;
        fp.yaw = kf.yaw; fp.pitch = kf.pitch;
        fp.ibl_rot = kf.ibl_rot; fp.ibl_int = kf.ibl_int;
        fp.emit_scale = kf.emit_scale;
    };

    if (t <= m_keyframes.front().time) { fill(fp, m_keyframes.front()); return fp; }
    if (t >= m_keyframes.back().time) { fill(fp, m_keyframes.back()); return fp; }

    int seg = 0;
    for (int i = 0; i < static_cast<int>(m_keyframes.size()) - 1; i++) {
        if (t >= m_keyframes[i].time && t < m_keyframes[i + 1].time) {
            seg = i;
            break;
        }
    }

    float seg_dur = m_keyframes[seg + 1].time - m_keyframes[seg].time;
    float u = (seg_dur > 0.0f) ? (t - m_keyframes[seg].time) / seg_dur : 0.0f;

    int i0 = std::max(seg - 1, 0);
    int i1 = seg;
    int i2 = seg + 1;
    int i3 = std::min(seg + 2, static_cast<int>(m_keyframes.size()) - 1);

    const Keyframe& k0 = m_keyframes[i0];
    const Keyframe& k1 = m_keyframes[i1];
    const Keyframe& k2 = m_keyframes[i2];
    const Keyframe& k3 = m_keyframes[i3];

    fp.x = catmullRom(k0.x, k1.x, k2.x, k3.x, u);
    fp.y = catmullRom(k0.y, k1.y, k2.y, k3.y, u);
    fp.z = catmullRom(k0.z, k1.z, k2.z, k3.z, u);
    fp.yaw = catmullRomAngle(k0.yaw, k1.yaw, k2.yaw, k3.yaw, u);
    fp.pitch = catmullRom(k0.pitch, k1.pitch, k2.pitch, k3.pitch, u);
    fp.ibl_rot = lerp(k1.ibl_rot, k2.ibl_rot, u);
    fp.ibl_int = lerp(k1.ibl_int, k2.ibl_int, u);
    fp.emit_scale = lerp(k1.emit_scale, k2.emit_scale, u);

    return fp;
}

float CameraPath::catmullRom(float p0, float p1, float p2, float p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

float CameraPath::catmullRomAngle(float a0, float a1, float a2, float a3, float t) {
    a0 = a1 + wrapAngle(a0 - a1);
    a2 = a1 + wrapAngle(a2 - a1);
    a3 = a1 + wrapAngle(a3 - a1);
    return wrapAngle(catmullRom(a0, a1, a2, a3, t));
}

float CameraPath::wrapAngle(float a) {
    while (a > 180.0f)  a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

float CameraPath::lerp(float a, float b, float t) {
    return a + t * (b - a);
}

} // namespace vt
