#pragma once

#include "geometry/shape.h"

namespace vt {

// Simple triangle (flat normal, no UVs). Used by legacy "mesh" scene command.
class Triangle : public Shape {
    static constexpr float EPS = 1e-7f;

public:
    vec3 v0, v1, v2;

    Triangle(const vec3& a, const vec3& b, const vec3& c, const Material* m)
        : Shape(m), v0(a), v1(b), v2(c) {}

    std::optional<Intersection> intersect(const Ray& r) const override;
    BoundingBox boundingBox() const override;
};

} // namespace vt
