#pragma once

#include <string>

namespace vkc {
class Application;
class Renderer;

enum window_type {
  AUTO = 0,
  KMS,
  XCB,
  WAYLAND,
};

class Window {
public:
  std::string name;

  Window() {}
  virtual ~Window() {}

  virtual int init(Application* app, Renderer *vc) = 0;
  virtual void loop(Application* app, Renderer *vc) = 0;
};
}
