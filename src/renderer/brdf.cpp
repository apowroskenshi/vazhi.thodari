#include "renderer/brdf.h"
#include "renderer/sampling.h"
#include "core/rng.h"
#include <cmath>
#include <algorithm>

namespace vt::brdf {

float distributionGgx(const vec3& N, const vec3& m, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;

    float m_dot_n = dot(m, N);
    if (m_dot_n <= 0.0f) return 0.0f;

    float m_dot_n2 = m_dot_n * m_dot_n;
    float denom = m_dot_n2 * (a2 - 1.0f) + 1.0f;
    denom = PI * denom * denom;

    return a2 / std::max(denom, 1e-7f);
}

float geometryGgx(const vec3& N, const vec3& v, const vec3& m, float roughness) {
    float v_dot_n = dot(v, N);
    float v_dot_m = dot(v, m);

    if (v_dot_m * v_dot_n <= 0.0f) return 0.0f;

    float abs_v_dot_n = std::abs(v_dot_n);
    float a = roughness * roughness;
    float k = a / 2.0f;

    float denom = abs_v_dot_n * (1.0f - k) + k;
    return abs_v_dot_n / std::max(denom, 1e-7f);
}

float geometrySmithGgx(const vec3& N, const vec3& L, const vec3& V,
                       const vec3& H, float roughness) {
    return geometryGgx(N, L, H, roughness) * geometryGgx(N, V, H, roughness);
}

vec3 fresnelSchlick(const vec3& L, const vec3& m, const Material& mat) {
    vec3 dielectric_f0 = vec3(std::pow((mat.ior - 1.0f) / (mat.ior + 1.0f), 2.0f));
    vec3 F0 = glm::mix(dielectric_f0, mat.Kd, mat.metalness);

    float d = std::abs(dot(L, m));
    return F0 + (vec3(1.0f) - F0) * std::pow(1.0f - d, 5.0f);
}

std::pair<float, float> computeEta(const vec3& out_dir, const vec3& N, const Material& mat) {
    float out_dot_n = dot(out_dir, N);
    if (out_dot_n > 0.0f)
        return {1.0f, mat.ior};
    else
        return {mat.ior, 1.0f};
}

vec3 sampleBrdf(const vec3& N, const vec3& V, const Material& mat) {
    float e = randomFloat();
    float s = length(mat.Kd) + length(mat.Ks) + length(mat.Kt);
    float pd = length(mat.Kd) / s;
    float ps = length(mat.Ks) / s;

    float e1 = randomFloat();
    float e2 = randomFloat() * TWO_PI;

    // Diffuse
    if (e < pd) {
        return sampleLobe(N, std::sqrt(e1), e2);
    }

    // GGX microfacet half-vector
    float a = mat.roughness * mat.roughness;
    float a2 = a * a;
    float cos_theta = std::sqrt((1.0f - e1) / (e1 * (a2 - 1.0f) + 1.0f));
    vec3 m = sampleLobe(N, cos_theta, e2);

    // Reflection
    if (e < pd + ps) {
        return 2.0f * dot(V, m) * m - V;
    }

    // Transmission
    auto eta = computeEta(V, N, mat);
    float ior = eta.first / eta.second;
    float out_dot_m = dot(V, m);
    float r = 1.0f - (ior * ior) * (1.0f - out_dot_m * out_dot_m);

    if (r < 0.0f) {
        return 2.0f * dot(V, m) * m - V; // TIR
    }

    int sign = dot(V, N) >= 0.0f ? 1 : -1;
    return (ior * out_dot_m - sign * std::sqrt(r)) * m - ior * V;
}

float pdfBrdf(const vec3& N, const vec3& L, const vec3& V, const Material& mat) {
    vec3 Nn = normalize(N);
    vec3 Ln = normalize(L);
    vec3 Vn = normalize(V);
    vec3 m = normalize(Ln + Vn);

    float s = length(mat.Kd) + length(mat.Ks) + length(mat.Kt);
    if (s <= 0.0f) return 0.0f;

    float pd = length(mat.Kd) / s;
    float pr = length(mat.Ks) / s;
    float pt = length(mat.Kt) / s;

    float l_dot_n = dot(Ln, Nn);
    bool is_transmission = (mat.Kt != vec3(0.0f));
    if (!is_transmission && l_dot_n <= 0.0f) return 0.0f;

    // Diffuse PDF
    float Pd = std::max(0.0f, l_dot_n) * INV_PI;

    // Specular PDF (GGX)
    float dm = distributionGgx(Nn, m, mat.roughness);
    float v_dot_m = dot(Vn, m);
    float Pr = (v_dot_m > 1e-3f) ? (dm * dot(m, Nn)) / (4.0f * v_dot_m) : 0.0f;

    // Transmission PDF
    float Pt = 0.0f;
    auto eta = computeEta(Vn, N, mat);
    float ni = eta.first;
    float no = eta.second;
    float ior = ni / no;

    vec3 tm = -1.0f * normalize(no * Ln + ni * Vn);
    float out_dot_m = dot(Vn, tm);
    float r = 1.0f - (ior * ior) * (1.0f - out_dot_m * out_dot_m);

    if (r < 0.0f) {
        Pt = Pr; // TIR
    } else {
        float L_dot_m = dot(Ln, tm);
        float V_dot_m = dot(Vn, tm);

        float denom = (no * L_dot_m) + (ni * V_dot_m);
        denom *= denom;

        float num = no * no * std::abs(L_dot_m);
        dm = distributionGgx(Nn, tm, mat.roughness);
        Pt = (denom > 1e-3f) ? (dm * dot(tm, Nn)) * (num / denom) : 0.0f;
    }

    return pd * Pd + pr * Pr + pt * Pt;
}

vec3 evalScattering(const vec3& N, const vec3& L, const vec3& V,
                    const Material& mat, float t) {
    vec3 Nn = normalize(N);
    vec3 Ln = normalize(L);
    vec3 Vn = normalize(V);
    vec3 m = normalize(Ln + Vn);

    float s = length(mat.Kd) + length(mat.Ks) + length(mat.Kt);
    float pd = length(mat.Kd) / s;
    float pr = length(mat.Ks) / s;
    float pt = length(mat.Kt) / s;

    float in_dot_n = dot(Ln, Nn);
    float out_dot_n = dot(Vn, Nn);

    bool is_transmission = (mat.Kt != vec3(0.0f));
    if (!is_transmission && (in_dot_n <= 0.0f || out_dot_n <= 0.0f))
        return vec3(0.0f);

    vec3 diffuse(0.0f), specular(0.0f), transmission(0.0f);

    // Diffuse (metals have no diffuse)
    if (pd > 0.0f) {
        vec3 F = fresnelSchlick(Ln, m, mat);
        diffuse = (1.0f - mat.metalness) * (vec3(1.0f) - F) * mat.Kd * INV_PI;
    }

    // Specular (GGX)
    if (pr > 0.0f) {
        float D = distributionGgx(Nn, m, mat.roughness);
        float G = geometrySmithGgx(Nn, Ln, Vn, m, mat.roughness);
        vec3 F = fresnelSchlick(Ln, m, mat);
        float denom = 4.0f * glm::max(out_dot_n, 0.0001f);
        specular = (D * G * F) / denom;
    }

    // Transmission (GGX)
    if (pt > 0.0f) {
        auto eta = computeEta(Vn, N, mat);
        float ni = eta.first;
        float no = eta.second;
        float ior = ni / no;

        vec3 tm = -1.0f * normalize(no * Ln + ni * Vn);
        float out_dot_m = dot(Vn, tm);
        float r = 1.0f - (ior * ior) * (1.0f - out_dot_m * out_dot_m);

        vec3 atten(1.0f);
        if (out_dot_n < 0.0f && t > 0.0f) {
            atten = glm::exp(t * glm::log(mat.Kt));
        }

        if (r < 0.0f) {
            // TIR
            float D = distributionGgx(Nn, m, mat.roughness);
            float G = geometrySmithGgx(Nn, Ln, Vn, m, mat.roughness);
            vec3 F = fresnelSchlick(Ln, m, mat);
            float denom = 4.0f * glm::max(std::abs(out_dot_n), 0.0001f);
            transmission = atten * (D * G * F) / denom;
        } else {
            float L_dot_m = dot(Ln, tm);
            float V_dot_m = dot(Vn, tm);

            float denom = (no * L_dot_m) + (ni * V_dot_m);
            denom *= denom;
            denom *= std::abs(out_dot_n);

            float D = distributionGgx(Nn, tm, mat.roughness);
            float G = geometrySmithGgx(Nn, Ln, Vn, tm, mat.roughness);
            vec3 F = vec3(1.0f) - fresnelSchlick(Ln, tm, mat);

            vec3 num = atten * D * G * F * std::abs(L_dot_m) * std::abs(V_dot_m) * (no * no);
            transmission = (denom > 1e-3f) ? (num / denom) : vec3(0.0f);
        }
    }

    return in_dot_n * diffuse + specular + transmission;
}

} // namespace vt::brdf
