#pragma once

namespace vik {
class Window {
public:

  enum window_type {
    AUTO = 0,
    KMS,
    XCB_SIMPLE,
    XCB_MOUSE,
    WAYLAND_XDG,
    WAYLAND_LEGACY,
    INVALID
  };

  Window() {}
  ~Window() {}
};
}
