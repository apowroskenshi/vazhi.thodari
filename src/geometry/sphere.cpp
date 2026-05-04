#include "geometry/sphere.h"

namespace vt {

std::optional<Intersection> Sphere::intersect(const Ray& r) const {
    vec3 oc = r.origin - center;

    float a = dot(r.direction, r.direction);
    float b = 2.0f * dot(oc, r.direction);
    float c = dot(oc, oc) - radius * radius;

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f)
        return std::nullopt;

    float sqrt_disc = std::sqrt(discriminant);
    float t_minus = (-b - sqrt_disc) / (2.0f * a);
    float t_plus = (-b + sqrt_disc) / (2.0f * a);

    // Find smallest positive t
    float t;
    vec3 normal;

    if (t_minus > EPSILON) {
        t = t_minus;
        normal = normalize(r.at(t) - center);
    } else if (t_plus > EPSILON) {
        t = t_plus;
        normal = normalize(r.at(t) - center);
    } else {
        return std::nullopt;
    }

    Intersection hit;
    hit.t = t;
    hit.position = r.at(t);
    hit.normal = normal;
    hit.shape = this;
    return hit;
}

BoundingBox Sphere::boundingBox() const {
    BoundingBox box(center - vec3(radius));
    box.extend(center + vec3(radius));
    return box;
}

} // namespace vt
