#pragma once

#include "core/math_types.h"

namespace vt {

struct Ray {
    vec3 origin;
    vec3 direction; // Always normalized

    Ray(const vec3& o, const vec3& d)
        : origin(o), direction(normalize(d)) {}

    vec3 at(float t) const {
        return origin + t * direction;
    }
};

} // namespace vt
