#pragma once

#include <string>


#include "vikWindow.hpp"

namespace vkc {
class Renderer;

class Window : public vik::Window {
public:
  Window() {}
  virtual ~Window() {}

  virtual int init(Renderer *r) = 0;
  virtual void iterate(Renderer *r) = 0;
};
}
