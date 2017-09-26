#pragma once

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#include <vector>

#include "vikSwapChain.hpp"
#include "system/vikSettings.hpp"
#include "vikDebug.hpp"

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

  Settings *settings;

  Renderer() {}
  ~Renderer() {}

  void set_settings(Settings *s) {
    settings = s;
  }

  void set_swap_chain(SwapChain *sc) {
    swap_chain = sc;
  }

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
};
}
