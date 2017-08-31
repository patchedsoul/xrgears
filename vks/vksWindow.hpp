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

#include "vksApplication.hpp"

class VikWindow {
 public:
  VikWindow() {}
  ~VikWindow() {}

  virtual int init(Application *app) = 0;
  virtual void loop(Application *app) = 0;
  virtual const char* requiredExtensionName() = 0;
  virtual void initSwapChain(const VkInstance &instance, VulkanSwapChain* swapChain) = 0;
};
