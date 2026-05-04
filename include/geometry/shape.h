#pragma once

#include "core/math_types.h"
#include "core/ray.h"
#include "core/intersection.h"
#include "geometry/bvh_box.h"
#include <optional>

namespace vt {

class Material;

class Shape {
public:
    const Material* mat = nullptr;

    Shape() = default;
    explicit Shape(const Material* m) : mat(m) {}
    virtual ~Shape() = default;

    virtual std::optional<Intersection> intersect(const Ray& r) const = 0;
    virtual BoundingBox boundingBox() const = 0;
};

} // namespace vt
