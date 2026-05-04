#pragma once

#include "core/math_types.h"
#include <optional>

namespace vt {

class Shape; // Forward declaration

struct Intersection {
    float t = INF;
    vec3 position{0.0f};
    vec3 normal{0.0f};
    std::optional<vec3> geometric_normal; // Face normal (Ng), set by MeshTriangle
    std::optional<vec3> tangent;          // For normal mapping
    const Shape* shape = nullptr;
    vec2 uv{0.0f, 0.0f};

    float distance() const { return t; }
};

struct Interval {
    float t_min = 0.0f;
    float t_max = INF;
    vec3 normal_min{0.0f};
    vec3 normal_max{0.0f};

    Interval() = default;

    Interval(float tmin, float tmax)
        : t_min(tmin), t_max(tmax) {}

    Interval(float tmin, float tmax, const vec3& nmin, const vec3& nmax)
        : t_min(tmin), t_max(tmax), normal_min(nmin), normal_max(nmax)
    {
        if (t_min > t_max) {
            std::swap(t_min, t_max);
            std::swap(normal_min, normal_max);
        }
    }

    void setEmpty() {
        t_min = 0.0f;
        t_max = -1.0f;
    }

    bool isEmpty() const { return t_min > t_max; }

    void intersect(const Interval& other) {
        if (other.t_min > t_min) {
            t_min = other.t_min;
            normal_min = other.normal_min;
        }
        if (other.t_max < t_max) {
            t_max = other.t_max;
            normal_max = other.normal_max;
        }
    }
};

} // namespace vt
