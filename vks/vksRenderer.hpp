#pragma once

#include <string>

#include <vulkan/vulkan.h>
//#include "../vks/vksWindow.hpp"
#include "../vks/vksSettings.hpp"

namespace vks {

class Window;

class Renderer {
public:
  // Vulkan instance, stores all per-application states
  VkInstance instance;

  Renderer();
  ~Renderer();
  VkResult createInstance(Settings* settings, Window *window, const std::string& name);

};
}
