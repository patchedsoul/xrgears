#pragma once

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#include <vector>

#include "vikSwapChain.hpp"
#include "system/vikSettings.hpp"
#include "vikDebug.hpp"
#include "window/vikWindow.hpp"
#include "vikInitializers.hpp"

namespace vik {

class Renderer {
public:
  VkInstance instance;
  VkDevice device;
  VkPhysicalDevice physical_device;

  VkCommandPool cmd_pool;

  std::vector<VkCommandBuffer> cmd_buffers;

  VkQueue queue;
  std::vector<VkFramebuffer> frame_buffers;
  VkRenderPass render_pass;

  uint32_t width;
  uint32_t height;

  Settings *settings;

  Window *window;

  Renderer(Settings *s, Window *w) {
    settings = s;
    window = w;
  }

  ~Renderer() {}

  VkResult create_instance(const std::string& name,
                           const std::vector<const char*> &window_extensions) {
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = name.c_str();
    app_info.pEngineName = "vitamin-k";
    app_info.apiVersion = VK_MAKE_VERSION(1, 0, 2);

    std::vector<const char*> extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };

    // Enable surface extensions depending on window system
    extensions.insert(extensions.end(),
                      window_extensions.begin(),
                      window_extensions.end());

    if (settings->validation)
      extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledExtensionCount = extensions.size();
    instance_info.ppEnabledExtensionNames = extensions.data();

    if (settings->validation) {
      instance_info.enabledLayerCount = debug::validationLayerCount;
      instance_info.ppEnabledLayerNames = debug::validationLayerNames;
    }

    return vkCreateInstance(&instance_info, nullptr, &instance);
  }

  void create_frame_buffer(VkFramebuffer *frame_buffer,
                           const std::vector<VkImageView> &attachments) {
    VkFramebufferCreateInfo frame_buffer_info = {};
    frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frame_buffer_info.renderPass = render_pass;
    frame_buffer_info.attachmentCount = attachments.size();
    frame_buffer_info.pAttachments = attachments.data();
    frame_buffer_info.width = width;
    frame_buffer_info.height = height;
    frame_buffer_info.layers = 1;

    vik_log_check(vkCreateFramebuffer(device,
                                      &frame_buffer_info,
                                      nullptr,
                                      frame_buffer));
  }

  void allocate_command_buffers() {
    vik_log_e("Allocating Command Buffers. %d", window->get_swap_chain()->image_count);
    // Create one command buffer for each swap chain image and reuse for rendering
    cmd_buffers.resize(window->get_swap_chain()->image_count);

    VkCommandBufferAllocateInfo cmd_buffer_info =
        initializers::commandBufferAllocateInfo(
          cmd_pool,
          VK_COMMAND_BUFFER_LEVEL_PRIMARY,
          static_cast<uint32_t>(cmd_buffers.size()));

    vik_log_check(vkAllocateCommandBuffers(device,
                                           &cmd_buffer_info,
                                           cmd_buffers.data()));
  }

};
}
