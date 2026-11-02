#include "lab_gui/screenshot_util.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <GLFW/glfw3.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace lab_gui {

bool ScreenshotUtil::capture_framebuffer_png(const std::string& filepath, int width, int height) {
    if (width <= 0 || height <= 0) return false;

    std::filesystem::path p(filepath);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    std::vector<unsigned char> pixels(width * height * 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // Invert rows vertically from bottom-left to top-left
    std::vector<unsigned char> flipped(width * height * 4);
    const int stride = width * 4;
    for (int y = 0; y < height; ++y) {
        const unsigned char* src_row = pixels.data() + (height - 1 - y) * stride;
        unsigned char* dst_row = flipped.data() + y * stride;
        std::copy(src_row, src_row + stride, dst_row);
    }

    const int result = stbi_write_png(filepath.c_str(), width, height, 4, flipped.data(), stride);
    if (result != 0) {
        std::cout << "[Screenshot] Successfully captured: " << filepath << " (" << width << "x" << height << ")\n";
        return true;
    } else {
        std::cerr << "[Screenshot] Failed to write PNG: " << filepath << "\n";
        return false;
    }
}

} // namespace lab_gui
