#pragma once

#include "core/math_types.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace vt {

// RAII texture: owns pixel data in a vector, supports bilinear sampling.
// Move-only (no accidental copies of multi-MB pixel buffers).
class Texture {
public:
    Texture() = default;
    explicit Texture(const std::string& path);

    // Move-only
    Texture(Texture&&) noexcept = default;
    Texture& operator=(Texture&&) noexcept = default;
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Bilinear-filtered RGB sample at UV (wrapped to [0,1])
    vec3 sample(const vec2& uv) const;

    // Single-channel (R) sample
    float sampleFloat(const vec2& uv) const { return sample(uv).x; }

    // Nearest-neighbor alpha sample (channel 3) for crisp cutouts
    float sampleAlpha(const vec2& uv) const;

    // Check if any pixel has non-opaque alpha
    bool hasAlpha() const;

    bool loaded() const { return !m_pixels.empty(); }
    int width() const { return m_width; }
    int height() const { return m_height; }
    const std::string& path() const { return m_path; }

    // Direct access for cache serialization
    const uint8_t* data() const { return m_pixels.data(); }
    size_t dataSize() const { return m_pixels.size(); }

    // Construct from raw pixel data (for cache loading)
    void loadFromMemory(int w, int h, int depth, const uint8_t* rgba_data,
                          size_t byte_count, const std::string& src_path);

private:
    std::vector<uint8_t> m_pixels; // RGBA, 4 bytes per pixel
    int m_width = 0;
    int m_height = 0;
    int m_depth = 0; // Original channel count before forced RGBA
    std::string m_path;
};

} // namespace vt
