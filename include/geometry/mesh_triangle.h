#pragma once

#include "geometry/shape.h"
#include "material/material.h"

namespace vt {

// Triangle with per-vertex normals, UVs, and tangents.
// Supports smooth shading, texture lookups, alpha testing, and normal mapping.
class MeshTriangle : public Shape {
    static constexpr float EPS = 1e-7f;

public:
    vec3 v0, v1, v2;        // Positions
    vec3 n0, n1, n2;        // Per-vertex normals
    vec2 uv0, uv1, uv2;    // Per-vertex UVs
    vec3 t0, t1, t2;        // Per-vertex tangents
    vec3 Ng;                 // Precomputed geometric face normal

    // With tangents
    MeshTriangle(const vec3& a, const vec3& b, const vec3& c,
                 const vec3& na, const vec3& nb, const vec3& nc,
                 const vec2& ta, const vec2& tb, const vec2& tc,
                 const vec3& tan0, const vec3& tan1, const vec3& tan2,
                 const Material* m);

    // Without tangents
    MeshTriangle(const vec3& a, const vec3& b, const vec3& c,
                 const vec3& na, const vec3& nb, const vec3& nc,
                 const vec2& ta, const vec2& tb, const vec2& tc,
                 const Material* m);

    std::optional<Intersection> intersect(const Ray& r) const override;
    BoundingBox boundingBox() const override;

    float area() const;
    vec3 computeTangent(const vec3& N) const;
};

} // namespace vt
