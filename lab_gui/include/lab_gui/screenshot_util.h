#pragma once

#include <string>

namespace lab_gui {

class ScreenshotUtil {
public:
    /**
     * \brief Read the current OpenGL backbuffer and save as PNG.
     * \param filepath Destination path (.png)
     * \param width Framebuffer width in pixels
     * \param height Framebuffer height in pixels
     * \return True if successful
     */
    static bool capture_framebuffer_png(const std::string& filepath, int width, int height);
};

} // namespace lab_gui
