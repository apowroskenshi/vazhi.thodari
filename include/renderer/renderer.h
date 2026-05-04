#pragma once

#include "core/math_types.h"
#include "core/ray.h"
#include "scene/scene.h"
#include "output/image_buffer.h"

namespace vt {

struct RenderConfig {
    int max_passes = 2048;
    int write_interval = 32;
    float russian_roulette_prob = 0.8f;
    float noise_threshold = 0.0005f;
    bool clamp_lights = false;
    float clamp_factor = 200.0f;
    bool clamp_weights = false;
    float weight_clamp_factor = 200.0f;
};

class Renderer {
public:
    Renderer(const Scene& scene, const RenderConfig& config = {});

    // Render one pass into the image buffer (accumulates).
    void traceImage(ImageBuffer& buffer, int pass);

    const RenderConfig& config() const { return m_config; }

private:
    Color tracePath(const Ray& ray) const;

    const Scene& m_scene;
    RenderConfig m_config;
};

} // namespace vt
