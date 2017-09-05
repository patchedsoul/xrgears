#include "vksRenderer.hpp"
#include "../vks/vksWindow.hpp"

namespace vks {

Renderer::Renderer() {

}
Renderer::~Renderer() {

}

/**
  * Create the application wide Vulkan instance
  *
  * @note Virtual, can be overriden by derived example class for custom instance creation
  */
VkResult Renderer::createInstance(Settings* settings, VikWindow *window, const std::string& name) {
  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = name.c_str();
  appInfo.pEngineName = name.c_str();
  appInfo.apiVersion = VK_API_VERSION_1_0;

  std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

  // Enable surface extensions depending on os
  std::string windowExtension = std::string(window->requiredExtensionName());

  if (!windowExtension.empty())
    instanceExtensions.push_back(window->requiredExtensionName());

  instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pNext = NULL;
  instanceCreateInfo.pApplicationInfo = &appInfo;
  if (instanceExtensions.size() > 0) {
    if (settings->validation)
      instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
  }
  if (settings->validation) {
    instanceCreateInfo.enabledLayerCount = vks::debug::validationLayerCount;
    instanceCreateInfo.ppEnabledLayerNames = vks::debug::validationLayerNames;
  }
  return vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
}
}
