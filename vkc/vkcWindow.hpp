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

  std::function<void()> init_cb;
  std::function<void()> update_cb;

  Window() {}
  virtual ~Window() {}

  void set_init_cb(std::function<void()> cb) {
    init_cb = cb;
  }

  void set_update_cb(std::function<void()> cb) {
    update_cb = cb;
  }

  virtual int init(Renderer *r) = 0;
  virtual void loop(Renderer *r) = 0;
};
}
