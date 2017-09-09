#pragma once

#include <string>
#include <vector>
#include <functional>

#include <vulkan/vulkan.h>

#include "vikRenderer.hpp"

#include "vksSettings.hpp"
#include "vksDevice.hpp"
#include "vksSwapChain.hpp"
#include "vksTimer.hpp"
#include "vksTextOverlay.hpp"

namespace vks {

class Window;

class Renderer : public vik::Renderer {
public:

  Timer timer;
  SwapChain swapChain;
  Device *vksDevice;
  Settings* settings;
  TextOverlay *textOverlay;

  VkInstance instance;

  VkPhysicalDevice physicalDevice;
  VkPhysicalDeviceProperties deviceProperties;
  VkPhysicalDeviceFeatures deviceFeatures;
  VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
  VkPhysicalDeviceFeatures enabledFeatures{};

  VkDevice device;
  VkQueue queue;
  VkFormat depthFormat;
  VkCommandPool cmdPool;
  VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submitInfo;
  VkRenderPass renderPass;
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
  std::vector<VkFramebuffer> frameBuffers;
  std::vector<VkShaderModule> shaderModules;

  std::vector<const char*> enabledExtensions;

  uint32_t destWidth;
  uint32_t destHeight;
  uint32_t width = 2560;
  uint32_t height = 1440;
  uint32_t currentBuffer = 0;

  bool enableTextOverlay = true;

  Renderer();
  ~Renderer();

  void createCommandPool();
  bool checkCommandBuffers();
  void createCommandBuffers();
  void destroyCommandBuffers();
  void createPipelineCache();
  void prepareFrame();
  void submitFrame();
  void init_text_overlay(const std::string &title);

  void init_physical_device();
  void init_debugging();
  void get_physical_device_properties();
  void init_semaphores();
  void list_gpus();

  VkResult createInstance(Window *window, const std::string& name);
  VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin);
  void initVulkan(Settings *settings, Window *window, const std::string& name);
  void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free);
  void check_tick_finnished(Window *window, const std::string &title);

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
