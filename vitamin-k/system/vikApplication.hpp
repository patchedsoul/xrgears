#pragma once

#include "vikSettings.hpp"

#include "window/vikWindowXCB.hpp"
#include "window/vikWindowWaylandXDG.hpp"
#include "window/vikWindowWaylandShell.hpp"
#include "window/vikWindowKMS.hpp"
#include "window/vikWindowKhrDisplay.hpp"

namespace vik {
class Application {
public:
    Settings settings;
    Window *window;
    bool quit = false;

    Application(int argc, char *argv[]) {
      if (!settings.parse_args(argc, argv))
        vik_log_f("Invalid arguments.");
    }

    ~Application()  {}

    int init_window_from_settings() {
      switch (settings.type) {
        case Settings::KMS:
          window = new WindowKMS(&settings);
          break;
        case Settings::XCB:
          window = new WindowXCB(&settings);
          break;
        case Settings::WAYLAND_XDG:
          window = new WindowWaylandXDG(&settings);
          break;
        case Settings::WAYLAND_SHELL:
          window = new WindowWaylandShell(&settings);
          break;
          break;
        case Settings::KHR_DISPLAY:
          window = new WindowKhrDisplay(&settings);
        case Settings::AUTO:
          return -1;
      }
    }
};
}
