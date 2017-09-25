#pragma once

#include <string>

#include "../vitamin-k/vikWindow.hpp"
#include "../vitamin-k/vikRenderer.hpp"

namespace vkc {
class Renderer;

class Window : public vik::Window {
public:
  Window() {}
  virtual ~Window() {}

  virtual int init(vik::Renderer *r) = 0;
  virtual void iterate(vik::Renderer *r) = 0;
};
}
