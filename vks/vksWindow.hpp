/*
 * XRGears
 *
 * Copyright (C) 2016 Sascha Willems - www.saschawillems.de
 * Copyright (C) 2017 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 *
 * Based on Vulkan Examples written by Sascha Willems
 */

#pragma once

#include <string>

#include "vikWindow.hpp"

#include "vksApplication.hpp"

namespace vks {

class Window : public vik::Window {
 public:
  Window() {}
  ~Window() {}

  virtual int init(Application *app) = 0;
  virtual void iter(Application *app) = 0;

  virtual const std::vector<const char*> required_extensions() = 0;
  virtual void init_swap_chain(Renderer *r) = 0;
  virtual void update_window_title(const std::string& title) = 0;
};
}
