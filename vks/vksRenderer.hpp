#pragma once

#include <string>

#include <vulkan/vulkan.h>
//#include "../vks/vksWindow.hpp"
#include "../vks/vksSettings.hpp"

namespace vks {

class VikWindow;

class Renderer {
public:
  // Vulkan instance, stores all per-application states
  VkInstance instance;

  Renderer();
  ~Renderer();
  VkResult createInstance(Settings* settings, VikWindow *window, const std::string& name);

};
}
