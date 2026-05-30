#include "renderer/renderer.h"
#include "renderer/brdf.h"
#include "renderer/sampling.h"
#include "core/rng.h"
#include "camera/camera.h"

#include <cmath>
#include <algorithm>
#include <thread>
#include <omp.h>

namespace vt {

Renderer::Renderer(const Scene& scene, const RenderConfig& config)
    : m_scene(scene), m_config(config) {}

Color Renderer::tracePath(const Ray& initial_ray) const {
    const float RR = m_config.russian_roulette_prob;

    Color C(0.0f);
    vec3 W(1.0f);

    Intersection P = m_scene.bvh->intersect(initial_ray);

    if (P.t >= INF) {
        if (m_scene.m_useIbl && m_scene.m_activeEnvMap) {
            //return m_scene.m_iblIntensity * m_scene.m_activeEnvMap->lookup(initial_ray.direction);
            return m_scene.m_activeEnvMap->lookup(initial_ray.direction);
        }
        return C;
    }

    if (P.shape->mat->isEmissive()) {
        vec3 emission = P.shape->mat->Kd;
        if (P.shape->mat->tex_emissive) {
            emission = P.shape->mat->tex_emissive->sample(P.uv);
        }
        return emission * m_scene.m_areaLightEmitScale;
    }

    vec3 out_dir = -initial_ray.direction;

    // Resolve material textures
    Material resolved;
    const Material* mat = P.shape->mat;
    if (mat->hasTextures()) {
        resolved = mat->resolve(P.uv);
        mat = &resolved;
    }

    // Normal map
    if (P.shape->mat->hasNormalMap() && P.tangent.has_value()) {
        P.normal = P.shape->mat->perturbNormal(P.normal, *P.tangent, P.uv);
    }

    while (randomFloat() <= RR) {
        vec3 N = P.normal;
        vec3 offset_N = P.geometric_normal.value_or(P.normal);

        // Explicit area light sampling
        if (!m_scene.m_areaLight.empty()) {
            Intersection L;
            L.t = 0.0f;

            auto ls = m_scene.m_areaLight.sample(randomFloat(), randomFloat(), randomFloat());
            L.position = ls.position;
            L.normal = ls.normal;
            L.shape = reinterpret_cast<const Shape*>(ls.tri);

            float p = m_scene.m_areaLight.pdf(P.position, L.position, L.normal);
            vec3 in_dir = normalize(L.position - P.position);
            float p_brdf = brdf::pdfBrdf(P.normal, in_dir, out_dir, *mat);
            float weight = (p * p) / (p * p + p_brdf * p_brdf);

            Ray shadow_ray(P.position + EPSILON * offset_N, in_dir);
            Intersection hit = m_scene.bvh->intersect(shadow_ray);

            if (p > 0 && hit.t < INF && hit.shape->mat->isEmissive()) {
                vec3 f = brdf::evalScattering(P.normal, in_dir, out_dir, *mat, P.t);

                vec3 light_emission = hit.shape->mat->Kd;
                if (hit.shape->mat->tex_emissive) {
                    light_emission = hit.shape->mat->tex_emissive->sample(hit.uv);
                }

                vec3 contrib = weight * W * (f / p) * light_emission * m_scene.m_areaLightEmitScale;

                if (m_config.clamp_lights) {
                    float max_c = glm::max(contrib.x, glm::max(contrib.y, contrib.z));
                    if (max_c > m_config.clamp_factor)
                        contrib *= m_config.clamp_factor / max_c;
                }

                C += contrib;
            }
        }

        // IBL explicit sampling
        if (m_scene.m_useIbl && m_scene.m_activeEnvMap) {
            auto ibl_sample = m_scene.m_activeEnvMap->sample(randomFloat(), randomFloat());

            if (ibl_sample.pdf > 0.0f) {
                float n_dot_l = dot(N, ibl_sample.direction);
                if (n_dot_l > 0.0f) {
                    Ray ibl_ray(P.position + EPSILON * offset_N, ibl_sample.direction);
                    Intersection ibl_hit = m_scene.bvh->intersect(ibl_ray);

                    if (ibl_hit.t >= INF) {
                        vec3 f_ibl = brdf::evalScattering(N, ibl_sample.direction, out_dir, *mat, P.t);
                        float p_ibl = ibl_sample.pdf;
                        float p_brdf_ibl = brdf::pdfBrdf(N, ibl_sample.direction, out_dir, *mat);
                        float w_ibl = (p_ibl * p_ibl) / (p_ibl * p_ibl + p_brdf_ibl * p_brdf_ibl);

                        C += W * w_ibl * (f_ibl / p_ibl) * m_scene.m_iblIntensity * ibl_sample.radiance;
                    }
                }
            }
        }

        // Extend path via BRDF sampling
        vec3 in_dir = brdf::sampleBrdf(N, out_dir, *mat);

        bool is_transmission = dot(in_dir, N) < 0.0f;
        Ray next_ray(P.position + (is_transmission ? -1.0f : 1.0f) * EPSILON * offset_N, in_dir);

        Intersection next_hit = m_scene.bvh->intersect(next_ray);

        // Escaped scene → IBL
        if (next_hit.t >= INF) {
            if (m_scene.m_useIbl && m_scene.m_activeEnvMap) {
                vec3 env_radiance = m_scene.m_iblIntensity * m_scene.m_activeEnvMap->lookup(in_dir);
                vec3 f = brdf::evalScattering(N, in_dir, out_dir, *mat, P.t);
                float p_brdf = brdf::pdfBrdf(N, in_dir, out_dir, *mat) * RR;

                if (p_brdf > EPSILON) {
                    float p_ibl = m_scene.m_activeEnvMap->pdf(in_dir);
                    float w_brdf = (p_brdf * p_brdf) / (p_brdf * p_brdf + p_ibl * p_ibl);
                    C += W * w_brdf * (f / p_brdf) * env_radiance;
                }
            }
            break;
        }

        vec3 f = brdf::evalScattering(N, in_dir, out_dir, *mat, P.t);
        float p = brdf::pdfBrdf(N, in_dir, out_dir, *mat) * RR;

        if (p < EPSILON) break;

        W *= (f / p);

        if (m_config.clamp_weights) {
            float max_w = glm::max(W.x, glm::max(W.y, W.z));
            if (max_w > m_config.weight_clamp_factor)
                W *= m_config.weight_clamp_factor / max_w;
        }

        // Implicit light connection
        if (next_hit.shape->mat->isEmissive()) {
            float p_brdf = brdf::pdfBrdf(N, in_dir, out_dir, *mat) * RR;
            float p_light = m_scene.m_areaLight.pdf(P.position, next_hit.position, next_hit.normal);
            float weight = (p_brdf * p_brdf) / (p_brdf * p_brdf + p_light * p_light);

            vec3 light_emission = next_hit.shape->mat->Kd;
            if (next_hit.shape->mat->tex_emissive) {
                light_emission = next_hit.shape->mat->tex_emissive->sample(next_hit.uv);
            }

            vec3 contrib = W * light_emission * weight * m_scene.m_areaLightEmitScale;

            if (m_config.clamp_lights) {
                float max_c = glm::max(contrib.x, glm::max(contrib.y, contrib.z));
                if (max_c > m_config.clamp_factor)
                    contrib *= m_config.clamp_factor / max_c;
            }

            C += contrib;
            break;
        }

        // Step forward
        P = next_hit;
        out_dir = -in_dir;
        mat = P.shape->mat;
        if (mat->hasTextures()) {
            resolved = mat->resolve(P.uv);
            mat = &resolved;
        }

        if (P.shape->mat->hasNormalMap() && P.tangent.has_value()) {
            P.normal = P.shape->mat->perturbNormal(P.normal, *P.tangent, P.uv);
        }
    }

    return C;
}

void Renderer::traceImage(ImageBuffer& buffer, int pass) {
    int width = buffer.width();
    int height = buffer.height();
    float aspect = static_cast<float>(width) / height;   

#pragma omp parallel for schedule(dynamic, 4) num_threads(28)
    for (int y = 0; y < height; y++) {        
        for (int x = 0; x < width; x++) {
            float u = (x + randomFloat()) / width;
            float v = (y + randomFloat()) / height;

            vec3 ray_dir = m_scene.camera->rayDirection(u, v, aspect);
            Ray ray(m_scene.camera->eye(), ray_dir);

            Color color = tracePath(ray);

            if (glm::any(glm::isnan(color)) || glm::any(glm::isinf(color))) {
                color = Color(0.0f);
            }

            buffer.accumulate(x, y, color);
        }
    }
}

} // namespace vt
