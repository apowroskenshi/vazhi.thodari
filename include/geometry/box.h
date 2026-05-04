#pragma once

#include "geometry/shape.h"

namespace vt {

class Box : public Shape {
public:
    vec3 corner;
    vec3 diagonal;

    Box(const vec3& corner, const vec3& diag, const Material* m)
        : Shape(m), corner(corner), diagonal(diag) {}

    std::optional<Intersection> intersect(const Ray& r) const override;
    BoundingBox boundingBox() const override;

private:
    Interval intersectSlab(const Ray& r, int axis) const;
};

} // namespace vt
