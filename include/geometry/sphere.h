#pragma once

#include "geometry/shape.h"

namespace vt {

class Sphere : public Shape {
public:
    vec3 center;
    float radius;

    Sphere(const vec3& c, float r, const Material* m)
        : Shape(m), center(c), radius(r) {}

    std::optional<Intersection> intersect(const Ray& r) const override;
    BoundingBox boundingBox() const override;
};

} // namespace vt
