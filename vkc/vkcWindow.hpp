#pragma once

#include <string>
#include <functional>

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

  virtual int init(Renderer* r, std::function<void()> app_init) = 0;
  virtual void loop(Application* app) = 0;
};
}
