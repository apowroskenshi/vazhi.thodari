#pragma once

#include "core/math_types.h"
#include <bvh/v2/bbox.h>

namespace vt {

// Bounding box wrapper around bvh::v2::BBox<float, 3>.
// Accepts glm::vec3 for convenience.
class BoundingBox : public bvh::v2::BBox<float, 3> {
public:
    using Base = bvh::v2::BBox<float, 3>;

    BoundingBox() : Base(Base::make_empty()) {}

    explicit BoundingBox(const vec3& v)
        : Base(to_bvh(v)) {}

    BoundingBox& extend(const vec3& v) {
        Base::extend(to_bvh(v));
        return *this;
    }

    BoundingBox& extend(const BoundingBox& other) {
        Base::extend(other);
        return *this;
    }

private:
    static bvh::v2::Vec<float, 3> to_bvh(const vec3& v) {
        return bvh::v2::Vec<float, 3>(v[0], v[1], v[2]);
    }
};

} // namespace vt
