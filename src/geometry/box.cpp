#include "geometry/box.h"
#include <cmath>

namespace vt {

Interval Box::intersectSlab(const Ray& r, int axis) const {
    vec3 N{0.0f};
    N[axis] = 1.0f;

    float d0 = -corner[axis];
    float d1 = -(corner[axis] + diagonal[axis]);
    if (d0 > d1) std::swap(d0, d1);

    float denom = N[axis] * r.direction[axis];

    if (std::abs(denom) > EPSILON) {
        float t0 = -(d0 + N[axis] * r.origin[axis]) / denom;
        float t1 = -(d1 + N[axis] * r.origin[axis]) / denom;
        if (t0 > t1) std::swap(t0, t1);

        vec3 n0{0.0f}; n0[axis] = -1.0f;
        vec3 n1{0.0f}; n1[axis] = 1.0f;
        return Interval(t0, t1, n0, n1);
    } else {
        float s0 = N[axis] * r.origin[axis] + d0;
        float s1 = N[axis] * r.origin[axis] + d1;
        if ((s0 > 0 && s1 < 0) || (s0 < 0 && s1 > 0)) {
            return Interval(); // Inside slab
        }
        Interval empty;
        empty.setEmpty();
        return empty;
    }
}

std::optional<Intersection> Box::intersect(const Ray& r) const {
    Interval interval;

    for (int i = 0; i < 3; ++i) {
        Interval slab = intersectSlab(r, i);
        if (slab.isEmpty()) return std::nullopt;

        interval.intersect(slab);
        if (interval.t_min > interval.t_max) return std::nullopt;
    }

    float t_hit = 0.0f;
    vec3 normal{0.0f};

    if (interval.t_min > EPSILON) {
        t_hit = interval.t_min;
        normal = interval.normal_min;
    } else if (interval.t_max > EPSILON) {
        t_hit = interval.t_max;
        normal = interval.normal_max;
    }

    if (t_hit > EPSILON && t_hit < INF) {
        Intersection hit;
        hit.t = t_hit;
        hit.position = r.at(t_hit);

        vec3 box_center = corner + diagonal * 0.5f;
        vec3 n = normalize(normal);
        if (dot(n, hit.position - box_center) < 0.0f)
            n = -n;

        hit.normal = n;
        hit.shape = this;
        return hit;
    }

    return std::nullopt;
}

BoundingBox Box::boundingBox() const {
    BoundingBox bbox(corner);
    bbox.extend(corner + diagonal);
    return bbox;
}

} // namespace vt
