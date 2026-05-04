#pragma once

#include "output/image_buffer.h"
#include <string>

namespace vt {

// Write image buffer as HDR (RGBE/Radiance) format.
void writeHdrImage(const std::string& path, const ImageBuffer& buffer);

// Write a raw Color array as HDR.
void writeHdrImage(const std::string& path, int width, int height, const Color* image);

} // namespace vt
