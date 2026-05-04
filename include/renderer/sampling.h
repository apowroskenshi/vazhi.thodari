#pragma once

#include "core/math_types.h"
#include "core/intersection.h"
#include "core/rng.h"
#include <cmath>
#include <utility>

namespace vt {

// Rotate vector K (expressed in local Z-up frame) to align with axis A.
inline vec3 sampleLobe(const vec3& A, float cos_theta, float phi) {
    float s = std::sqrt(1.0f - cos_theta * cos_theta);
    vec3 K(s * std::cos(phi), s * std::sin(phi), cos_theta);

    if (std::abs(A.z - 1.0f) < 1e-3f) {
        return K;
    } else if (std::abs(A.z + 1.0f) < 1e-3f) {
        return vec3(K.x, -K.y, -K.z);
    } else {
        vec3 B = normalize(vec3(-A.y, A.x, 0.0f));
        vec3 C = cross(A, B);
        return K.x * B + K.y * C + K.z * A;
    }
}

// Uniform sample on a sphere of given center and radius.
// Returns (position, normal).
inline std::pair<vec3, vec3> sampleSphere(const vec3& center, float radius) {
    float e1 = randomFloat();
    float e2 = randomFloat();

    float z = 2.0f * e1 - 1.0f;
    float r = std::sqrt(1.0f - z * z);
    float a = TWO_PI * e2;

    vec3 N(r * std::cos(a), r * std::sin(a), z);
    vec3 P = center + radius * N;
    return {P, N};
}

// Geometry factor between two intersection points.
inline float geometryFactor(const Intersection& A, const Intersection& B) {
    vec3 D = A.position - B.position;
    float num = dot(A.normal, D) * dot(B.normal, D);
    float denom = std::pow(dot(D, D), 2.0f);

    if (denom < 1e-6f) return 1e-6f;
    return std::abs(num / denom);
}

} // namespace vt
