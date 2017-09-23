#pragma once

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#include "vkcSwapChainVK.hpp"
#include "vkcSwapChainDRM.hpp"
#include "vikRenderer.hpp"

namespace vkc {

class Renderer : public vik::Renderer {
public:
  SwapChain *swap_chain;

  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkRenderPass render_pass;
  VkQueue queue;
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
  VkDeviceMemory mem;
  VkBuffer buffer;
  VkDescriptorSet descriptor_set;
  VkSemaphore semaphore;
  VkFence fence;
  VkCommandPool cmd_pool;

  uint32_t width, height;

  timeval start_tv;
  VkSurfaceKHR surface;
  VkFormat image_format;

  VkCommandBuffer cmd_buffer;

  uint32_t vertex_offset, colors_offset, normals_offset;

  Renderer(uint32_t w, uint32_t h) ;
  ~Renderer() ;

  void init_vk(const char *extension);
  VkFormat choose_surface_format();
  void init_render_pass();
  void init_vk_objects();
  void submit_queue();

  void create_vulkan_swapchain();

  void wait_and_reset_fences();

  uint64_t get_animation_time();

  void build_command_buffer(VkFramebuffer frame_buffer);

  void render(uint32_t index);

  void render_swapchain_vk();
};
}
