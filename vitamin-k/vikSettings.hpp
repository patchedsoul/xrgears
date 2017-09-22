#pragma once

#include <stdint.h>

#include "vikWindow.hpp"

namespace vik {
class Settings {
public:
    /** @brief Activates validation layers (and message output) when set to true */
    bool validation = false;
    /** @brief Set to true if fullscreen mode has been requested via command line */
    bool fullscreen = false;
    /** @brief Set to true if v-sync will be forced for the swapchain */
    bool vsync = false;

    uint32_t gpu_index = 0;

    bool list_gpus_and_exit = false;

    enum Window::window_type type = Window::AUTO;
};
}
