#pragma once

#include "core/math_types.h"

namespace vt {

class Camera {
public:
    Camera(const vec3& eye, const quat& orientation, float ry)
        : m_eye(eye), m_ry(ry), m_orientation(orientation) {}

    vec3 eye() const { return m_eye; }

    vec3 rayDirection(float u, float v, float aspect_ratio) const {
        float x = 2.0f * u - 1.0f;
        float y = 2.0f * v - 1.0f;

        x *= m_ry * aspect_ratio;
        y *= m_ry;

        vec3 dir = normalize(vec3(x, y, -1.0f));
        return rotate(m_orientation, dir);
    }

private:
    vec3 m_eye;
    float m_ry;
    quat m_orientation;
};

} // namespace vt
