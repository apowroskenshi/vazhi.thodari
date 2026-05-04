#include "geometry/cylinder.h"
#include <cmath>

namespace vt {

std::optional<Intersection> Cylinder::intersect(const Ray& r) const {
    float axis_length = length(axis);
    vec3 axis_norm = normalize(axis);
    vec3 z_axis{0.0f, 0.0f, 1.0f};

    // Rotation from Z-axis to cylinder axis
    quat rot = fromTwoVectors(z_axis, axis_norm);
    mat4 rot_mat4 = toMat4(rot);
    mat3 rot_mat = mat3(rot_mat4);
    mat3 inv_rot = glm::transpose(rot_mat);

    // Transform ray to cylinder space
    vec3 oc = r.origin - base;
    vec3 oc_t = inv_rot * oc;
    vec3 dir_t = inv_rot * r.direction;

    // End-cap slab (z=0 to z=axis_length)
    Interval cap_interval;
    cap_interval.normal_min = vec3(0.0f, 0.0f, -1.0f);
    cap_interval.normal_max = vec3(0.0f, 0.0f, 1.0f);

    float denom = dir_t.z;
    if (std::abs(denom) > EPSILON) {
        float t0 = -oc_t.z / denom;
        float t1 = (axis_length - oc_t.z) / denom;
        if (t0 > t1) {
            std::swap(t0, t1);
            std::swap(cap_interval.normal_min, cap_interval.normal_max);
        }
        cap_interval.t_min = t0;
        cap_interval.t_max = t1;
    } else {
        float s0 = oc_t.z;
        float s1 = oc_t.z - axis_length;
        if (!((s0 > 0 && s1 < 0) || (s0 < 0 && s1 > 0)))
            return std::nullopt;
    }

    // Cylinder body (infinite cylinder in XY)
    float a_coeff = dir_t.x * dir_t.x + dir_t.y * dir_t.y;
    float b_coeff = 2.0f * (oc_t.x * dir_t.x + oc_t.y * dir_t.y);
    float c_coeff = oc_t.x * oc_t.x + oc_t.y * oc_t.y - radius * radius;
    float disc = b_coeff * b_coeff - 4.0f * a_coeff * c_coeff;

    if (disc < 0.0f) return std::nullopt;

    Interval body_interval;
    if (std::abs(a_coeff) > EPSILON) {
        float sqrt_disc = std::sqrt(disc);
        float t_minus = (-b_coeff - sqrt_disc) / (2.0f * a_coeff);
        float t_plus = (-b_coeff + sqrt_disc) / (2.0f * a_coeff);

        body_interval.t_min = t_minus;
        body_interval.t_max = t_plus;

        vec3 p_min = oc_t + t_minus * dir_t;
        body_interval.normal_min = normalize(vec3(p_min.x, p_min.y, 0.0f));

        vec3 p_max = oc_t + t_plus * dir_t;
        body_interval.normal_max = normalize(vec3(p_max.x, p_max.y, 0.0f));
    } else {
        float dist_to_axis = std::sqrt(oc_t.x * oc_t.x + oc_t.y * oc_t.y);
        if (dist_to_axis > radius) return std::nullopt;
    }

    // Intersect the two intervals
    Interval interval = cap_interval;
    interval.intersect(body_interval);
    if (interval.t_min > interval.t_max) return std::nullopt;

    float t;
    vec3 normal_t;
    if (interval.t_min > EPSILON) {
        t = interval.t_min;
        normal_t = interval.normal_min;
    } else if (interval.t_max > EPSILON) {
        t = interval.t_max;
        normal_t = interval.normal_max;
    } else {
        return std::nullopt;
    }

    Intersection hit;
    hit.t = t;
    hit.position = r.at(t);
    hit.normal = normalize(rot_mat * normal_t);
    hit.shape = this;
    return hit;
}

BoundingBox Cylinder::boundingBox() const {
    vec3 top = base + axis;
    BoundingBox box(base - vec3(radius));
    box.extend(base + vec3(radius));
    box.extend(top - vec3(radius));
    box.extend(top + vec3(radius));
    return box;
}

} // namespace vt
