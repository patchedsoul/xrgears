#pragma once

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#include <vector>

namespace vik {
class Renderer {
public:
  VkInstance instance;
  VkDevice device;
  VkPhysicalDevice physical_device;

  std::vector<VkFramebuffer> frame_buffers;
  VkRenderPass render_pass;

  Renderer() {}
  ~Renderer() {}
};
}
