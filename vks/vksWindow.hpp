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

class Window {
 public:
  Window() {}
  ~Window() {}

  virtual int init(vks::Application *app) = 0;

  virtual const char* requiredExtensionName() = 0;
  virtual void initSwapChain(const VkInstance &instance, vks::SwapChain* swapChain) = 0;
  virtual void update_window_title(const std::string& title) = 0;
  virtual void flush(Application *app) = 0;
};
}
