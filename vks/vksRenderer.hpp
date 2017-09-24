#pragma once

#include <string>
#include <vector>
#include <functional>

#include <vulkan/vulkan.h>

#include "vikRenderer.hpp"

#include "vksDevice.hpp"
#include "vksSwapChain.hpp"
#include "vksTimer.hpp"
#include "vksTextOverlay.hpp"
#include "vikSettings.hpp"

namespace vks {

class Window;

class Renderer : public vik::Renderer {
public:
  Timer timer;
  Device *vksDevice;
  TextOverlay *textOverlay;

  VkPhysicalDeviceProperties deviceProperties;
  VkPhysicalDeviceFeatures deviceFeatures;
  VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
  VkPhysicalDeviceFeatures enabledFeatures{};

  VkFormat depthFormat;
  VkCommandPool cmdPool;
  VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submitInfo;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  VkPipelineCache pipelineCache;

  VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

  struct {
    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;
  } depthStencil;

  struct {
    VkSemaphore presentComplete;
    VkSemaphore renderComplete;
    VkSemaphore textOverlayComplete;
  } semaphores;

  std::vector<VkCommandBuffer> drawCmdBuffers;


  std::vector<const char*> enabledExtensions;

  uint32_t destWidth;
  uint32_t destHeight;
  uint32_t currentBuffer = 0;

  bool enableTextOverlay = true;

  vik::Settings *settings;

  Renderer();
  ~Renderer();

  void set_settings(vik::Settings *s);

  void prepare();

  void createCommandPool();
  bool checkCommandBuffers();
  void createCommandBuffers();
  void destroyCommandBuffers();
  void createPipelineCache();
  void prepareFrame();
  void submitFrame();
  void setupDepthStencil();
  void create_frame_buffers();
  void create_render_pass();

  void init_text_overlay();

  void init_physical_device();
  void init_debugging();
  void get_physical_device_properties();
  void init_semaphores();
  void list_gpus();

  VkResult create_instance(const std::string& name, const std::vector<const char *> &extensions);
  VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin);
  void initVulkan(const std::string& name, const std::vector<const char *> &extensions);
  void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free);
  void check_tick_finnished(Window *window, const std::string &title);

  void resize();

  float get_aspect_ratio();

  void updateTextOverlay(const std::string &title);
  void submit_text_overlay();

  std::string make_title_string(const std::string &title);

  std::function<void()> window_resize_cb;
  std::function<void()> enabled_features_cb;

  void set_window_resize_cb(std::function<void()> cb) {
    window_resize_cb = cb;
  }

  void set_enabled_features_cb(std::function<void()> cb) {
    enabled_features_cb = cb;
  }


};
}
