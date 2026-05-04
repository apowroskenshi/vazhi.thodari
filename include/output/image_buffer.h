#pragma once

#include "core/math_types.h"
#include <vector>
#include <cmath>

namespace vt {

class ImageBuffer {
public:
    ImageBuffer(int w, int h)
        : m_width(w), m_height(h),
          m_pixels(w * h, Color(0.0f)),
          m_pixelsSq(w * h, Color(0.0f)),
          m_passCount(0) {}

    void accumulate(int x, int y, Color c) {
        int idx = y * m_width + x;
        m_pixels[idx] += c;
        m_pixelsSq[idx] += c * c;
    }

    Color normalized(int x, int y) const {
        if (m_passCount == 0) return Color(0.0f);
        return m_pixels[y * m_width + x] / static_cast<float>(m_passCount);
    }

    Color raw(int x, int y) const {
        return m_pixels[y * m_width + x];
    }

    void incrementPass() { m_passCount++; }
    int passCount() const { return m_passCount; }

    float noiseEstimate() const {
        if (m_passCount < 2) return 1.0f;
        float total_variance = 0.0f;
        int n = m_width * m_height;
        float p = static_cast<float>(m_passCount);

        for (int j = 0; j < n; j++) {
            vec3 mean = m_pixels[j] / p;
            vec3 mean_sq = m_pixelsSq[j] / p;
            vec3 variance = mean_sq - mean * mean;
            total_variance += 0.2126f * variance.r
                            + 0.7152f * variance.g
                            + 0.0722f * variance.b;
        }
        return std::sqrt(total_variance / n / p);
    }

    void clear() {
        std::fill(m_pixels.begin(), m_pixels.end(), Color(0.0f));
        std::fill(m_pixelsSq.begin(), m_pixelsSq.end(), Color(0.0f));
        m_passCount = 0;
    }

    int width() const { return m_width; }
    int height() const { return m_height; }

    // Access normalized image as contiguous array (for writers)
    std::vector<Color> normalizedImage() const {
        std::vector<Color> out(m_width * m_height);
        float p = static_cast<float>(m_passCount);
        if (p > 0) {
            for (int j = 0; j < m_width * m_height; j++)
                out[j] = m_pixels[j] / p;
        }
        return out;
    }

private:
    int m_width, m_height;
    std::vector<Color> m_pixels;
    std::vector<Color> m_pixelsSq;
    int m_passCount;
};

} // namespace vt
