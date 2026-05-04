#include "geometry/triangle.h"

namespace vt {

std::optional<Intersection> Triangle::intersect(const Ray& r) const {
    // Moller-Trumbore
    vec3 e1 = v1 - v0;
    vec3 e2 = v2 - v0;
    vec3 h = cross(r.direction, e2);
    float a = dot(e1, h);

    if (std::abs(a) < EPS) return std::nullopt;

    float f = 1.0f / a;
    vec3 s = r.origin - v0;
    float u = f * dot(s, h);
    if (u < 0.0f || u > 1.0f) return std::nullopt;

    vec3 q = cross(s, e1);
    float v = f * dot(r.direction, q);
    if (v < 0.0f || u + v > 1.0f) return std::nullopt;

    float t = f * dot(e2, q);
    if (t < EPS) return std::nullopt;

    Intersection hit;
    hit.t = t;
    hit.position = r.at(t);
    hit.normal = normalize(cross(e1, e2));
    hit.shape = this;
    return hit;
}

BoundingBox Triangle::boundingBox() const {
    BoundingBox box;
    box.extend(v0);
    box.extend(v1);
    box.extend(v2);
    return box;
}

} // namespace vt
