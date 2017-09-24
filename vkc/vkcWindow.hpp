#pragma once

#include <string>
#include <functional>

#include "vikWindow.hpp"

namespace vkc {
class Renderer;

class Window : public vik::Window {
public:
  std::string name;

  std::function<void()> init_cb;
  std::function<void()> update_cb;
  std::function<void()> quit_cb;

  Window() {}
  virtual ~Window() {}

  void set_init_cb(std::function<void()> cb) {
    init_cb = cb;
  }

  void set_update_cb(std::function<void()> cb) {
    update_cb = cb;
  }

  void set_quit_cb(std::function<void()> cb) {
    quit_cb = cb;
  }

  virtual int init(Renderer *r) = 0;
  virtual void iterate(Renderer *r) = 0;
};
}
