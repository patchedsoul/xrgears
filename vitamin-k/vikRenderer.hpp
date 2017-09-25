#pragma once

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#include <vector>

#include "vikSwapChain.hpp"

namespace vik {

class Settings;

class Renderer {
public:
  VkInstance instance;
  VkDevice device;
  VkPhysicalDevice physical_device;

  VkQueue queue;
  std::vector<VkFramebuffer> frame_buffers;
  VkRenderPass render_pass;

  uint32_t width;
  uint32_t height;

  SwapChain *swap_chain;

  Settings *settings;

  Renderer() {}
  ~Renderer() {}

  void set_settings(Settings *s) {
    settings = s;
  }
};
}
