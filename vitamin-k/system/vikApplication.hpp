#pragma once

#include "vikSettings.hpp"

#include "window/vikWindowXCBSimple.hpp"
#include "window/vikWindowXCBInput.hpp"
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

    Application() {}
    ~Application()  {}

    int init_window_from_settings() {
      switch (settings.type) {
        case Settings::KMS:
          window = new WindowKMS();
          break;
        case Settings::XCB_SIMPLE:
          window = new WindowXCBSimple();
          break;
        case Settings::WAYLAND_XDG:
          window = new WindowWaylandXDG();
          break;
        case Settings::WAYLAND_LEGACY:
          window = new WindowWaylandShell();
          break;
        case Settings::XCB_MOUSE:
          window = new WindowXCBInput();
          break;
        case Settings::KHR_DISPLAY:
          window = new WindowKhrDisplay();
        case Settings::AUTO:
          return -1;
      }
    }
};
}
