#pragma once

#include "core/math_types.h"
#include "material/texture.h"
#include <memory>

namespace vt {

struct Material {
    vec3 Kd{1.0f, 0.5f, 0.0f}; // Diffuse reflectance
    vec3 Ks{1.0f, 1.0f, 1.0f}; // Specular reflectance
    vec3 Kt{0.0f};              // Transmission (Beer's law)
    float alpha = 1.0f;         // Phong exponent (legacy, converted from roughness)
    float ior = 1.5f;           // Index of refraction
    float roughness = 0.5f;     // GGX roughness [0=mirror, 1=diffuse]
    float metalness = 0.0f;     // 0=dielectric, 1=metal
    bool thin_shell = false;    // Thin-shell transmission
    bool emissive = false;      // If true, Kd is emission radiance
    bool skip_geometry = false;  // If true, loader skips geometry for this material

    // Texture maps (shared — multiple materials may reference the same texture)
    std::shared_ptr<Texture> tex;              // map_Kd — diffuse color
    std::shared_ptr<Texture> tex_ks;           // map_Ks — specular color
    std::shared_ptr<Texture> tex_alpha;        // map_Ns — roughness
    std::shared_ptr<Texture> tex_bump;         // map_Bump/Kn — normal map
    std::shared_ptr<Texture> tex_opacity;      // map_d — opacity
    std::shared_ptr<Texture> tex_emissive;     // map_Ke — emissive color
    std::shared_ptr<Texture> tex_metal_roughness; // glTF combined (G=rough, B=metal)

    bool isEmissive() const { return emissive; }

    bool hasTextures() const {
        return tex || tex_ks || tex_alpha || tex_bump || tex_opacity || tex_emissive;
    }

    bool hasNormalMap() const { return tex_bump != nullptr; }

    // Resolve all textures at a UV coordinate, returning a baked-out material.
    // The returned material has no texture pointers set.
    Material resolve(const vec2& uv) const;

    // Perturb shading normal using the normal map in tangent space.
    vec3 perturbNormal(const vec3& N, const vec3& T, const vec2& uv) const;

    // Factory helpers
    static Material diffuse(const vec3& kd, const vec3& ks, float a) {
        Material m;
        m.Kd = kd; m.Ks = ks; m.alpha = a;
        return m;
    }

    static Material withTransmission(const vec3& kd, const vec3& ks, float a,
                                      const vec3& kt, float ior_val) {
        Material m;
        m.Kd = kd; m.Ks = ks; m.alpha = a; m.Kt = kt; m.ior = ior_val;
        return m;
    }

    static Material light(const vec3& emission) {
        Material m;
        m.Kd = emission;
        m.emissive = true;
        return m;
    }
};

} // namespace vt
