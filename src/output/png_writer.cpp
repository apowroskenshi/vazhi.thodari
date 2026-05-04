#include "output/png_writer.h"

#include "stb_image_write.h"

#include <vector>
#include <algorithm>
#include <cmath>

namespace vt {

vec3 acesTonemap(vec3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return glm::clamp(
        (x * (a * x + vec3(b))) / (x * (c * x + vec3(d)) + vec3(e)),
        vec3(0.0f), vec3(1.0f));
}

void writePngImage(const std::string& path, int width, int height,
                   const Color* image, float exposure) {
    std::vector<unsigned char> pixels(width * height * 3);

    for (int j = 0; j < width * height; j++) {
        vec3 color = image[j] * exposure;
        color = acesTonemap(color);
        color = glm::pow(glm::max(color, vec3(0.0f)), vec3(1.0f / 2.2f));

        pixels[j * 3 + 0] = static_cast<unsigned char>(std::min(color.r * 255.0f + 0.5f, 255.0f));
        pixels[j * 3 + 1] = static_cast<unsigned char>(std::min(color.g * 255.0f + 0.5f, 255.0f));
        pixels[j * 3 + 2] = static_cast<unsigned char>(std::min(color.b * 255.0f + 0.5f, 255.0f));
    }

    stbi_flip_vertically_on_write(1);
    stbi_write_png(path.c_str(), width, height, 3, pixels.data(), width * 3);
}

void writePngImage(const std::string& path, const ImageBuffer& buffer, float exposure) {
    auto normalized = buffer.normalizedImage();
    writePngImage(path, buffer.width(), buffer.height(), normalized.data(), exposure);
}

} // namespace vt
