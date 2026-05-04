#pragma once

#include "core/math_types.h"
#include <vector>
#include <string>

namespace vt {

// HDR environment map with importance sampling via 2D CDF.
// Stored in equirectangular (lat-long) projection.
class EnvironmentMap {
public:
    struct IBLSample {
        vec3 direction;
        float pdf = 0.0f;
        vec3 radiance{0.0f};
    };

    EnvironmentMap() = default;

    bool load(const std::string& path);
    void build(); // Build importance sampling tables from luminance

    vec3 lookup(const vec3& direction) const;
    IBLSample sample(float e1, float e2) const;
    float pdf(const vec3& direction) const;

    bool isLoaded() const { return m_loaded; }
    int width() const { return m_width; }
    int height() const { return m_height; }

    float rotation_offset = 0.0f;

private:
    vec3 getPixel(int x, int y) const;

    std::vector<float> m_pixels; // RGB floats, row-major, top-to-bottom
    int m_width = 0;
    int m_height = 0;

    std::vector<float> m_marginalCdf;
    std::vector<float> m_marginalPdf;
    std::vector<std::vector<float>> m_conditionalCdf;
    std::vector<std::vector<float>> m_conditionalPdf;
    float m_totalPower = 0.0f;
    bool m_loaded = false;
};

} // namespace vt
