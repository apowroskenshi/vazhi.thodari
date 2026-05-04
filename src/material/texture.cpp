#include "material/texture.h"

#include "stb_image.h"

#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cmath>

namespace vt {

Texture::Texture(const std::string& path) {
    // Normalize path separators
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    std::ifstream test(normalized);
    if (test.fail()) {
        throw std::runtime_error("Texture file not found: " + normalized);
    }

    stbi_set_flip_vertically_on_load(true);
    int w, h, channels;
    unsigned char* data = stbi_load(normalized.c_str(), &w, &h, &channels, 4);
    if (!data) {
        throw std::runtime_error(
            "Failed to load texture " + normalized + ": " + stbi_failure_reason());
    }

    m_width = w;
    m_height = h;
    m_depth = channels;
    m_path = normalized;

    size_t byte_count = static_cast<size_t>(w) * h * 4;
    m_pixels.assign(data, data + byte_count);
    stbi_image_free(data);
}

void Texture::loadFromMemory(int w, int h, int depth, const uint8_t* rgba_data,
                               size_t byte_count, const std::string& src_path) {
    m_width = w;
    m_height = h;
    m_depth = depth;
    m_path = src_path;
    m_pixels.assign(rgba_data, rgba_data + byte_count);
}

vec3 Texture::sample(const vec2& uv) const {
    if (m_pixels.empty()) return vec3(1.0f);

    float u = uv.x - std::floor(uv.x);
    float v = uv.y - std::floor(uv.y);

    float fx = u * m_width - 0.5f;
    float fy = v * m_height - 0.5f;

    int x0 = static_cast<int>(std::floor(fx));
    int y0 = static_cast<int>(std::floor(fy));
    float dx = fx - x0;
    float dy = fy - y0;

    auto texel = [&](int x, int y) -> vec3 {
        x = ((x % m_width) + m_width) % m_width;
        y = ((y % m_height) + m_height) % m_height;
        int idx = (y * m_width + x) * 4;
        return vec3(m_pixels[idx] / 255.0f,
                    m_pixels[idx + 1] / 255.0f,
                    m_pixels[idx + 2] / 255.0f);
    };

    return (1 - dx) * (1 - dy) * texel(x0, y0) +
           dx * (1 - dy) * texel(x0 + 1, y0) +
           (1 - dx) * dy * texel(x0, y0 + 1) +
           dx * dy * texel(x0 + 1, y0 + 1);
}

float Texture::sampleAlpha(const vec2& uv) const {
    if (m_pixels.empty()) return 1.0f;

    float u = uv.x - std::floor(uv.x);
    float v = uv.y - std::floor(uv.y);

    int x = std::min(static_cast<int>(u * m_width), m_width - 1);
    int y = std::min(static_cast<int>(v * m_height), m_height - 1);
    int idx = (y * m_width + x) * 4;
    return m_pixels[idx + 3] / 255.0f;
}

bool Texture::hasAlpha() const {
    if (m_pixels.empty()) return false;
    for (int i = 0; i < m_width * m_height; i++) {
        if (m_pixels[i * 4 + 3] < 255) return true;
    }
    return false;
}

} // namespace vt
