#include "material/material.h"
#include <algorithm>

namespace vt {

Material Material::resolve(const vec2& uv) const {
    Material resolved = *this;

    if (tex) {
        vec3 tex_color = tex->sample(uv);
        resolved.Kd = tex_color;

        // If no specular texture, tint Ks with diffuse for metallic reflections
        if (!tex_ks) {
            resolved.Ks = Ks * tex_color;
        }
    }

    if (tex_ks) {
        resolved.Ks = tex_ks->sample(uv);
    }

    if (tex_alpha) {
        resolved.alpha = alpha * tex_alpha->sampleFloat(uv);
        resolved.alpha = glm::max(resolved.alpha, 1.0f);
    }

    // Clear texture pointers on resolved copy — it's fully baked
    resolved.tex = nullptr;
    resolved.tex_ks = nullptr;
    resolved.tex_alpha = nullptr;
    resolved.tex_bump = nullptr;
    resolved.tex_opacity = nullptr;

    return resolved;
}

vec3 Material::perturbNormal(const vec3& N, const vec3& T, const vec2& uv) const {
    if (!tex_bump) return N;

    // Orthogonalize tangent with respect to normal
    vec3 tangent = normalize(T - dot(T, N) * N);
    vec3 bitangent = cross(N, tangent);

    // Sample normal map: RGB [0,1] -> XYZ [-1,1]
    vec3 tex_normal = tex_bump->sample(uv) * 2.0f - vec3(1.0f);

    // Transform from tangent space to world space
    vec3 world_normal = normalize(
        tex_normal.x * tangent +
        tex_normal.y * bitangent +
        tex_normal.z * N
    );

    // Prevent flipping below the surface
    if (dot(world_normal, N) < 0.1f) {
        world_normal = N;
    }

    return world_normal;
}

} // namespace vt
