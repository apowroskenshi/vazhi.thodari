#pragma once

#include "geometry/shape.h"

namespace vt {

class Cylinder : public Shape {
public:
    vec3 base;
    vec3 axis;
    float radius;

    Cylinder(const vec3& b, const vec3& a, float r, const Material* m)
        : Shape(m), base(b), axis(a), radius(r) {}

    std::optional<Intersection> intersect(const Ray& r) const override;
    BoundingBox boundingBox() const override;
};

} // namespace vt
