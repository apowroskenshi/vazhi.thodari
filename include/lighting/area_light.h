#pragma once

#include "core/math_types.h"
#include "geometry/mesh_triangle.h"
#include <vector>
#include <algorithm>

namespace vt {

// Area light composed of emissive triangles with CDF-based importance sampling.
class AreaLight {
public:
    struct Sample {
        vec3 position;
        vec3 normal;
        float pdf = 0.0f;
        const MeshTriangle* tri = nullptr;
    };

    AreaLight() = default;

    void addTriangle(const MeshTriangle* tri);
    void build(); // Build CDF after all triangles added

    Sample sample(float e1, float e2, float e3) const;
    float pdf() const;
    bool empty() const { return m_triangles.empty(); }
    size_t triangleCount() const { return m_triangles.size(); }
    float totalArea() const { return m_totalArea; }

private:
    std::vector<const MeshTriangle*> m_triangles;
    std::vector<float> m_cdf;
    float m_totalArea = 0.0f;
};

} // namespace vt
