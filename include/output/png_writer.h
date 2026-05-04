#pragma once

#include "output/image_buffer.h"
#include <string>

namespace vt {

// ACES filmic tonemapping
vec3 acesTonemap(vec3 x);

// Write image buffer as tonemapped PNG.
void writePngImage(const std::string& path, const ImageBuffer& buffer, float exposure = 1.0f);

// Write a raw Color array as tonemapped PNG.
void writePngImage(const std::string& path, int width, int height,
                   const Color* image, float exposure = 1.0f);

} // namespace vt
