#pragma once

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#include "vkcSwapChainVK.hpp"
#include "vkcSwapChainDRM.hpp"

namespace vkc {

class Renderer {
public:
  SwapChain *swap_chain_obj;

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


  int current;

  VkCommandBuffer cmd_buffer;
  bool cmd_buffer_created = false;

  uint32_t vertex_offset, colors_offset, normals_offset;

  Renderer(uint32_t w, uint32_t h) ;
  ~Renderer() ;

  void init_vk(const char *extension);
  VkFormat choose_surface_format();
  void init_render_pass();
  void init_vk_objects();

  VkResult aquire_next_image(uint32_t *index);

  void create_swapchain_if_needed();
  void submit_queue();

  void wait_and_reset_fences();

  uint64_t get_animation_time();

  void build_command_buffer(VkFramebuffer frame_buffer);

  void render(uint32_t index);

  void render_swapchain_vk();
  void render_swapchain_drm();

  void init_buffer(RenderBuffer *b);
};
}
