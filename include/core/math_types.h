#pragma once

// Core math types and constants for the path tracer.
// Thin wrapper around GLM with project-wide aliases.

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_CTOR_INIT
#define GLM_FORCE_RADIANS
#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/quaternion.hpp>

namespace vt {

using glm::vec2;
using glm::vec3;
using glm::ivec3;
using glm::vec4;
using glm::mat3;
using glm::mat4;
using glm::quat;

using glm::dot;
using glm::normalize;
using glm::cross;
using glm::inverse;
using glm::length;
using glm::scale;
using glm::rotate;
using glm::conjugate;
using glm::toMat4;
using glm::angleAxis;

using Color = vec3;

inline constexpr float PI = 3.14159265358979f;
inline constexpr float INV_PI = 1.0f / PI;
inline constexpr float TWO_PI = 2.0f * PI;
inline constexpr float RADIANS = PI / 180.0f;
inline constexpr float EPSILON = 1e-4f;
inline constexpr float INF = 1e8f;

// Quaternion from two unit vectors
inline quat fromTwoVectors(const vec3& a, const vec3& b) {
    float w = 1.0f + dot(a, b);
    vec3 xyz;
    if (w < 1e-6f) {
        w = 0.0f;
        xyz = (std::abs(a.x) > std::abs(a.z))
            ? vec3(-a.y, a.x, 0.0f)
            : vec3(0.0f, -a.z, a.y);
    } else {
        xyz = cross(a, b);
    }
    return glm::normalize(quat(w, xyz.x, xyz.y, xyz.z));
}

inline vec3 transformVector(const quat& q, const vec3& v) {
    return glm::rotate(q, v);
}

inline mat4 translate(const vec3& v) {
    return glm::translate(glm::mat4(1.0f), v);
}

} // namespace vt
