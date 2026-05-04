#include "lighting/environment_map.h"
#include "stb_image.h"

#include <cmath>
#include <algorithm>
#include <cstdio>

namespace vt {

bool EnvironmentMap::load(const std::string& path) {
    int n;
    stbi_set_flip_vertically_on_load(false);
    float* data = stbi_loadf(path.c_str(), &m_width, &m_height, &n, 3);
    stbi_set_flip_vertically_on_load(true); // Restore for other textures

    if (!data) {
        fprintf(stderr, "Failed to load HDR environment map: %s\n", path.c_str());
        return false;
    }

    m_pixels.assign(data, data + m_width * m_height * 3);
    stbi_image_free(data);

    m_loaded = true;
    return true;
}

void EnvironmentMap::build() {
    if (!m_loaded) return;

    std::vector<float> luminance(m_width * m_height);

    m_marginalPdf.resize(m_height, 0.0f);
    m_conditionalCdf.resize(m_height);
    m_conditionalPdf.resize(m_height);
    m_totalPower = 0.0f;

    for (int y = 0; y < m_height; y++) {
        float theta = PI * (y + 0.5f) / m_height;
        float sin_theta = std::sin(theta);

        m_conditionalPdf[y].resize(m_width);
        m_conditionalCdf[y].resize(m_width);
        float row_sum = 0.0f;

        for (int x = 0; x < m_width; x++) {
            int idx = (y * m_width + x) * 3;
            float lum = 0.2126f * m_pixels[idx] +
                        0.7152f * m_pixels[idx + 1] +
                        0.0722f * m_pixels[idx + 2];
            float weighted = lum * sin_theta;

            luminance[y * m_width + x] = weighted;
            row_sum += weighted;
        }

        m_marginalPdf[y] = row_sum;
        m_totalPower += row_sum;

        float cumulative = 0.0f;
        for (int x = 0; x < m_width; x++) {
            m_conditionalPdf[y][x] = (row_sum > 0.0f)
                ? luminance[y * m_width + x] / row_sum
                : 1.0f / m_width;
            cumulative += luminance[y * m_width + x];
            m_conditionalCdf[y][x] = (row_sum > 0.0f)
                ? cumulative / row_sum
                : static_cast<float>(x + 1) / m_width;
        }
    }

    m_marginalCdf.resize(m_height);
    float cumulative = 0.0f;
    for (int y = 0; y < m_height; y++) {
        m_marginalPdf[y] = (m_totalPower > 0.0f)
            ? m_marginalPdf[y] / m_totalPower
            : 1.0f / m_height;
        cumulative += m_marginalPdf[y];
        m_marginalCdf[y] = cumulative;
    }
}

vec3 EnvironmentMap::lookup(const vec3& direction) const {
    if (!m_loaded) return vec3(0.0f);

    float theta = std::acos(std::clamp(direction.y, -1.0f, 1.0f));
    float phi = std::atan2(direction.z, direction.x);

    float u = phi / TWO_PI + 0.5f + rotation_offset / TWO_PI;
    u = u - std::floor(u);
    float v = theta / PI;

    float fx = u * m_width - 0.5f;
    float fy = v * m_height - 0.5f;

    int x0 = static_cast<int>(std::floor(fx));
    int y0 = static_cast<int>(std::floor(fy));
    float dx = fx - x0;
    float dy = fy - y0;

    auto wrap_x = [&](int x) { return ((x % m_width) + m_width) % m_width; };
    auto clamp_y = [&](int y) { return std::max(0, std::min(m_height - 1, y)); };

    vec3 c00 = getPixel(wrap_x(x0), clamp_y(y0));
    vec3 c10 = getPixel(wrap_x(x0 + 1), clamp_y(y0));
    vec3 c01 = getPixel(wrap_x(x0), clamp_y(y0 + 1));
    vec3 c11 = getPixel(wrap_x(x0 + 1), clamp_y(y0 + 1));

    return (1 - dx) * (1 - dy) * c00 + dx * (1 - dy) * c10 +
           (1 - dx) * dy * c01 + dx * dy * c11;
}

EnvironmentMap::IBLSample EnvironmentMap::sample(float e1, float e2) const {
    IBLSample result;
    result.direction = vec3(0, 1, 0);

    if (!m_loaded || m_totalPower <= 0.0f) return result;

    // Sample row from marginal CDF
    auto row_it = std::lower_bound(m_marginalCdf.begin(), m_marginalCdf.end(), e1);
    int y = std::min(static_cast<int>(std::distance(m_marginalCdf.begin(), row_it)),
                     m_height - 1);

    // Sample column from conditional CDF
    auto col_it = std::lower_bound(m_conditionalCdf[y].begin(),
                                   m_conditionalCdf[y].end(), e2);
    int x = std::min(static_cast<int>(std::distance(m_conditionalCdf[y].begin(), col_it)),
                     m_width - 1);

    float u = (x + 0.5f) / m_width;
    float v = (y + 0.5f) / m_height;

    float theta = v * PI;
    float phi = (u - 0.5f) * TWO_PI - rotation_offset;
    float sin_theta = std::sin(theta);

    result.direction = vec3(
        sin_theta * std::cos(phi),
        std::cos(theta),
        sin_theta * std::sin(phi));

    float pdf_uv = m_marginalPdf[y] * m_conditionalPdf[y][x] * m_width * m_height;
    result.pdf = (sin_theta > 1e-6f) ? pdf_uv / (TWO_PI * PI * sin_theta) : 0.0f;
    result.radiance = getPixel(x, y);
    return result;
}

float EnvironmentMap::pdf(const vec3& direction) const {
    if (!m_loaded || m_totalPower <= 0.0f) return 0.0f;

    float theta = std::acos(std::clamp(direction.y, -1.0f, 1.0f));
    float phi = std::atan2(direction.z, direction.x);

    float u = phi / TWO_PI + 0.5f + rotation_offset / TWO_PI;
    u = u - std::floor(u);
    float v = theta / PI;

    int x = std::min(static_cast<int>(u * m_width), m_width - 1);
    int y = std::min(static_cast<int>(v * m_height), m_height - 1);

    float sin_theta = std::sin(theta);
    if (sin_theta < 1e-6f) return 0.0f;

    float pdf_uv = m_marginalPdf[y] * m_conditionalPdf[y][x] * m_width * m_height;
    return pdf_uv / (TWO_PI * PI * sin_theta);
}

vec3 EnvironmentMap::getPixel(int x, int y) const {
    int idx = (y * m_width + x) * 3;
    return vec3(m_pixels[idx], m_pixels[idx + 1], m_pixels[idx + 2]);
}

} // namespace vt
