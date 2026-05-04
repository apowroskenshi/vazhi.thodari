#pragma once

#include "core/math_types.h"
#include "material/material.h"
#include <utility>

namespace vt::brdf {

// GGX/Trowbridge-Reitz NDF
float distributionGgx(const vec3& N, const vec3& m, float roughness);

// Schlick-GGX geometry term (single direction)
float geometryGgx(const vec3& N, const vec3& v, const vec3& m, float roughness);

// Smith G2 = G1(L) * G1(V)
float geometrySmithGgx(const vec3& N, const vec3& L, const vec3& V,
                         const vec3& H, float roughness);

// Fresnel-Schlick with metalness-aware F0
vec3 fresnelSchlick(const vec3& L, const vec3& m, const Material& mat);

// IOR helpers
std::pair<float, float> computeEta(const vec3& out_dir, const vec3& N, const Material& mat);

// Full BRDF evaluation at a surface point
vec3 evalScattering(const vec3& N, const vec3& L, const vec3& V,
                     const Material& mat, float t);

// Importance-sample the BRDF (returns sampled direction)
vec3 sampleBrdf(const vec3& N, const vec3& V, const Material& mat);

// PDF of a BRDF sample
float pdfBrdf(const vec3& N, const vec3& L, const vec3& V, const Material& mat);

} // namespace vt::brdf
