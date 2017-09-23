#pragma once

#include <vulkan/vulkan.h>

#include "vikSwapchain.hpp"

#define MAX_NUM_IMAGES 4

namespace vkc {

struct RenderBuffer {
  VkImage image;
  VkImageView view;
  VkFramebuffer framebuffer;
};

class SwapChain : public vik::SwapChain {
public:
  RenderBuffer buffers[MAX_NUM_IMAGES];

  SwapChain() {}
  ~SwapChain() {}

  /*
  void init_buffer(VkDevice device, VkFormat image_format, VkRenderPass render_pass,
                   uint32_t width, uint32_t height, RenderBuffer *b) {
    create_image_view(device, b->image, image_format, &b->view);
    create_frame_buffer(device, render_pass, &b->view, width, height, &b->framebuffer);
  }
  */
};
}
