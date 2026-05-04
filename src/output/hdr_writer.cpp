#include "output/hdr_writer.h"
#include "rgbe.h"

#include <vector>
#include <cstdio>

namespace vt {

void writeHdrImage(const std::string& path, int width, int height, const Color* image) {
    // Convert bottom-up to top-down float array
    std::vector<float> data(width * height * 3);
    float* dp = data.data();
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            Color pixel = image[y * width + x];
            *dp++ = pixel[0];
            *dp++ = pixel[1];
            *dp++ = pixel[2];
        }
    }

    rgbe_header_info info;
    char errbuf[100] = {0};

    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open HDR file for writing: %s\n", path.c_str());
        return;
    }

    info.valid = false;
    int r = RGBE_WriteHeader(fp, width, height, &info, errbuf);
    if (r != RGBE_RETURN_SUCCESS) {
        fprintf(stderr, "HDR header error: %s\n", errbuf);
        fclose(fp);
        return;
    }

    r = RGBE_WritePixels_RLE(fp, data.data(), width, height, errbuf);
    if (r != RGBE_RETURN_SUCCESS) {
        fprintf(stderr, "HDR write error: %s\n", errbuf);
    }
    fclose(fp);
}

void writeHdrImage(const std::string& path, const ImageBuffer& buffer) {
    auto normalized = buffer.normalizedImage();
    writeHdrImage(path, buffer.width(), buffer.height(), normalized.data());
}

} // namespace vt
