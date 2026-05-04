#include "geometry/bvh_accel.h"

#include <bvh/v2/sweep_sah_builder.h>
#include <bvh/v2/mini_tree_builder.h>
#include <span>

namespace vt {

namespace {

bvh::v2::Vec<float, 3> toBvh(const vec3& v) {
    return bvh::v2::Vec<float, 3>(v[0], v[1], v[2]);
}

vec3 fromBvh(const bvh::v2::Vec<float, 3>& v) {
    return vec3(v[0], v[1], v[2]);
}

Ray fromBvhRay(const bvh::v2::Ray<float, 3>& r) {
    return Ray(fromBvh(r.org), fromBvh(r.dir));
}

bvh::v2::Ray<float, 3> toBvhRay(const Ray& r) {
    return bvh::v2::Ray<float, 3>(toBvh(r.origin), toBvh(r.direction));
}

} // anonymous namespace

// BvhShape

BoundingBox BvhShape::boundingBox() const {
    return shape->boundingBox();
}

bvh::v2::Vec<float, 3> BvhShape::center() const {
    return boundingBox().get_center();
}

std::optional<Intersection> BvhShape::intersect(const bvh::v2::Ray<float, 3>& bvh_ray) const {
    Ray ray = fromBvhRay(bvh_ray);
    auto result = shape->intersect(ray);

    if (!result) return std::nullopt;
    if (result->t < bvh_ray.tmin || result->t > bvh_ray.tmax) return std::nullopt;

    return result;
}

// AccelerationBvh

AccelerationBvh::AccelerationBvh(const std::vector<std::unique_ptr<Shape>>& shapes) {
    m_shapes.reserve(shapes.size());
    for (const auto& s : shapes) {
        m_shapes.emplace_back(s.get());
    }

    std::vector<bvh::v2::BBox<float, 3>> bboxes;
    std::vector<bvh::v2::Vec<float, 3>> centers;
    bboxes.reserve(m_shapes.size());
    centers.reserve(m_shapes.size());

    for (const auto& shape : m_shapes) {
        bboxes.push_back(shape.boundingBox());
        centers.push_back(shape.center());
    }

    bvh::v2::ThreadPool thread_pool;
    m_bvh = bvh::v2::MiniTreeBuilder<Node>::build(
        thread_pool,
        std::span<const bvh::v2::BBox<float, 3>>(bboxes),
        std::span<const bvh::v2::Vec<float, 3>>(centers));
}

Intersection AccelerationBvh::intersect(const Ray& ray) const {
    bvh::v2::Ray<float, 3> bvh_ray = toBvhRay(ray);

    Intersection closest;
    closest.t = FLT_MAX;

    bvh::v2::SmallStack<Node::Index, 64> stack;

    m_bvh.intersect<false, false>(
        bvh_ray,
        m_bvh.get_root().index,
        stack,
        [&](size_t first_id, size_t last_id) {
            for (size_t i = first_id; i < last_id; ++i) {
                auto hit = m_shapes[m_bvh.prim_ids[i]].intersect(bvh_ray);
                if (hit && hit->t < closest.t) {
                    closest = *hit;
                }
            }
            return false; // Continue traversal to find closest hit
        });

    return closest;
}

} // namespace vt
