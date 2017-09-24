#pragma once

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#include <vector>

#include "vikSwapChain.hpp"

namespace vik {
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

  Renderer() {}
  ~Renderer() {}
};
}
