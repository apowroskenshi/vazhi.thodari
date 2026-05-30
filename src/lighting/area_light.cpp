#include "lighting/area_light.h"
#include <cmath>

namespace vt {

void AreaLight::addTriangle(const MeshTriangle* tri) {
    m_triangles.push_back(tri);
}

void AreaLight::build() {
    if (m_triangles.empty()) return;

    m_cdf.resize(m_triangles.size());
    m_totalArea = 0.0f;

    for (size_t i = 0; i < m_triangles.size(); i++) {
        m_totalArea += m_triangles[i]->area();
        m_cdf[i] = m_totalArea;
    }

    for (size_t i = 0; i < m_cdf.size(); i++) {
        m_cdf[i] /= m_totalArea;
    }
}

AreaLight::Sample AreaLight::sample(float e1, float e2, float e3) const {
    Sample s;
    if (m_triangles.empty() || m_totalArea <= 0.0f) return s;

    // Pick triangle proportional to area via CDF
    auto it = std::lower_bound(m_cdf.begin(), m_cdf.end(), e1);
    size_t idx = std::min(
        static_cast<size_t>(std::distance(m_cdf.begin(), it)),
        m_triangles.size() - 1);

    const MeshTriangle* tri = m_triangles[idx];
    s.tri = tri;

    // Uniform sample on triangle (sqrt mapping for uniform barycentric)
    float su = std::sqrt(e2);
    float bary_u = 1.0f - su;
    float bary_v = e3 * su;
    float bary_w = 1.0f - bary_u - bary_v;

    s.position = bary_w * tri->v0 + bary_u * tri->v1 + bary_v * tri->v2;
    s.normal = normalize(bary_w * tri->n0 + bary_u * tri->n1 + bary_v * tri->n2);
    s.pdf = 1.0f / m_totalArea;

    return s;
}

float AreaLight::pdf() const {
    if (m_totalArea <= 0.0f) return 0.0f;
    return 1.0f / m_totalArea;
}

float AreaLight::pdf(const vec3& shading_pos, const vec3& light_pos, const vec3& light_normal) const {
    if (m_totalArea <= 0.0f) return 0.0f;
    
    vec3 D = shading_pos - light_pos;
    
    float r2 = dot(D, D);
    if (r2 < 1e-8f) return 0.0f;
    
    float cos_light = std::abs(dot(light_normal, D)) / std::sqrt(r2);
    if (cos_light < 1e-6f) return 0.0f;
    
    return (1.0f / m_totalArea) * r2 / cos_light;
}

} // namespace vt
