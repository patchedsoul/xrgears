#pragma once

namespace vks {
class Settings {
public:
    /** @brief Activates validation layers (and message output) when set to true */
    bool validation = true;
    /** @brief Set to true if fullscreen mode has been requested via command line */
    bool fullscreen = false;
    /** @brief Set to true if v-sync will be forced for the swapchain */
    bool vsync = false;
};
}
