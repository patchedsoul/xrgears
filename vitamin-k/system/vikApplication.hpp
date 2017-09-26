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
        case Window::KMS:
          window = new WindowKMS();
          break;
        case Window::XCB_SIMPLE:
          window = new WindowXCBSimple();
          break;
        case Window::WAYLAND_XDG:
          window = new WindowWaylandXDG();
          break;
        case Window::WAYLAND_LEGACY:
          window = new WindowWaylandShell();
          break;
        case Window::XCB_MOUSE:
          window = new WindowXCBInput();
          break;
        case Window::KHR_DISPLAY:
          window = new WindowKhrDisplay();
        case Window::AUTO:
          return -1;
      }
    }
};
}
