#pragma once

#include <string>

#include <vulkan/vulkan.h>
#include "../vks/vksSettings.hpp"
#include "../vitamin-k/vikRenderer.hpp"

namespace vks {

class Window;

class Renderer : public vik::Renderer {
public:
  // Vulkan instance, stores all per-application states
  VkInstance instance;

  Renderer();
  ~Renderer();
  VkResult createInstance(Settings* settings, Window *window, const std::string& name);

};
}
