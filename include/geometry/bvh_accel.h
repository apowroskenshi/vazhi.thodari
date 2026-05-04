#pragma once

#include <span>
#include <vector>
#include <optional>
#include <memory>

#include <bvh/v2/bvh.h>
#include <bvh/v2/vec.h>
#include <bvh/v2/ray.h>
#include <bvh/v2/stack.h>
#include <bvh/v2/thread_pool.h>
#include <bvh/v2/sweep_sah_builder.h>
#include <bvh/v2/mini_tree_builder.h>

#include "core/math_types.h"
#include "core/ray.h"
#include "core/intersection.h"
#include "geometry/shape.h"
#include "geometry/bvh_box.h"

namespace vt {

bvh::v2::Vec<float, 3> vec3ToBvh(const vec3& v);
vec3 vec3FromBvh(const bvh::v2::Vec<float, 3>& v);
bvh::v2::Ray<float, 3> RayToBvh(const Ray& r);
Ray RayFromBvh(const bvh::v2::Ray<float, 3>& r);

class BvhShape {
public:
    Shape* shape;
    BvhShape(Shape* s) : shape(s) {}

    BoundingBox boundingBox() const;
    bvh::v2::Vec<float, 3> center() const;
    std::optional<Intersection> intersect(const bvh::v2::Ray<float, 3>& ray) const;
};

class AccelerationBvh {
    using Node = bvh::v2::Node<float, 3>;
    using BvhType = bvh::v2::Bvh<Node>;

    BvhType m_bvh;
    std::vector<BvhShape> m_shapes;

public:
    AccelerationBvh(const std::vector<std::unique_ptr<Shape>>& objs);
    Intersection intersect(const Ray& ray) const;
};

} // namespace vt
