#include "geometry/mesh_triangle.h"
#include <cmath>

namespace vt {

MeshTriangle::MeshTriangle(const vec3& a, const vec3& b, const vec3& c,
                           const vec3& na, const vec3& nb, const vec3& nc,
                           const vec2& ta, const vec2& tb, const vec2& tc,
                           const vec3& tan0, const vec3& tan1, const vec3& tan2,
                           const Material* m)
    : Shape(m), v0(a), v1(b), v2(c),
      n0(na), n1(nb), n2(nc),
      uv0(ta), uv1(tb), uv2(tc),
      t0(tan0), t1(tan1), t2(tan2)
{
    vec3 cp = cross(v1 - v0, v2 - v0);
    float len = length(cp);
    Ng = (len > 1e-8f) ? cp / len : vec3(0, 1, 0);
}

MeshTriangle::MeshTriangle(const vec3& a, const vec3& b, const vec3& c,
                           const vec3& na, const vec3& nb, const vec3& nc,
                           const vec2& ta, const vec2& tb, const vec2& tc,
                           const Material* m)
    : Shape(m), v0(a), v1(b), v2(c),
      n0(na), n1(nb), n2(nc),
      uv0(ta), uv1(tb), uv2(tc),
      t0(0.0f), t1(0.0f), t2(0.0f)
{
    vec3 cp = cross(v1 - v0, v2 - v0);
    float len = length(cp);
    Ng = (len > 1e-8f) ? cp / len : vec3(0, 1, 0);
}

std::optional<Intersection> MeshTriangle::intersect(const Ray& r) const {
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

    float w = 1.0f - u - v;

    // Alpha test on diffuse texture
    if (mat && mat->tex) {
        vec2 hit_uv = w * uv0 + u * uv1 + v * uv2;
        if (mat->tex->sampleAlpha(hit_uv) < 0.5f)
            return std::nullopt;
    }

    // Alpha test on opacity map
    if (mat && mat->tex_opacity) {
        vec2 hit_uv = w * uv0 + u * uv1 + v * uv2;
        if (mat->tex_opacity->sampleFloat(hit_uv) < 0.5f)
            return std::nullopt;
    }

    Intersection hit;
    hit.t = t;
    hit.position = r.at(t);
    hit.geometric_normal = Ng;

    // Interpolate per-vertex normals
    vec3 interp_n = w * n0 + u * n1 + v * n2;
    float len = length(interp_n);
    hit.normal = (len > 1e-6f) ? interp_n / len : Ng;

    // Ensure shading normal faces the incoming ray
    if (dot(hit.normal, r.direction) > 0.0f)
        hit.normal = -hit.normal;

    hit.uv = w * uv0 + u * uv1 + v * uv2;

    // Compute tangent for normal mapping
    if (mat && mat->hasNormalMap()) {
        hit.tangent = computeTangent(hit.normal);
    }

    hit.shape = this;
    return hit;
}

float MeshTriangle::area() const {
    return 0.5f * length(cross(v1 - v0, v2 - v0));
}

BoundingBox MeshTriangle::boundingBox() const {
    BoundingBox box;
    box.extend(v0);
    box.extend(v1);
    box.extend(v2);
    return box;
}

vec3 MeshTriangle::computeTangent(const vec3& N) const {
    vec3 e1 = v1 - v0;
    vec3 e2 = v2 - v0;
    vec2 duv1 = uv1 - uv0;
    vec2 duv2 = uv2 - uv0;

    float denom = duv1.x * duv2.y - duv2.x * duv1.y;
    if (std::abs(denom) < 1e-8f) {
        vec3 up = (std::abs(N.y) < 0.9f) ? vec3(0, 1, 0) : vec3(1, 0, 0);
        return normalize(cross(up, N));
    }

    float f = 1.0f / denom;
    vec3 T = f * (duv2.y * e1 - duv1.y * e2);
    return normalize(T - dot(T, N) * N);
}

} // namespace vt
