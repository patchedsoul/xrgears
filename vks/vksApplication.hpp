/*
* Vulkan Example base class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <sys/stat.h>

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include <iostream>
#include <chrono>
#include <string>
#include <array>
#include <vector>

#include "vksTools.hpp"
#include "vksDebug.hpp"
#include "vksInitializers.hpp"
#include "vksDevice.hpp"
#include "vksSwapChain.hpp"
#include "vksTextOverlay.hpp"
#include "vksCamera.hpp"
#include "vksRenderer.hpp"
#include "vksTimer.hpp"
#include "vksSettings.hpp"

#include "../vitamin-k/vikApplication.hpp"

namespace vks {
class Window;

class Application : public vik::Application {

 public:
  Renderer *renderer;
  Timer timer;
  SwapChain swapChain;
  Device *vksDevice;
  Settings settings;
  TextOverlay *textOverlay;
  Camera camera;

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
  std::vector<VkFramebuffer>frameBuffers;
  std::vector<VkShaderModule> shaderModules;

  std::vector<const char*> enabledExtensions;

  uint32_t destWidth;
  uint32_t destHeight;
  uint32_t width = 2560;
  uint32_t height = 1440;
  uint32_t currentBuffer = 0;

  bool prepared = false;
  bool viewUpdated = false;
  bool resizing = false;
  bool enableTextOverlay = true;

  float zoom = 0;
  float rotationSpeed = 1.0f;
  float zoomSpeed = 1.0f;

  glm::vec3 rotation = glm::vec3();
  glm::vec3 cameraPos = glm::vec3();
  glm::vec2 mousePos;

  std::string title = "Vulkan Example";
  std::string name = "vulkanExample";

  bool quit = false;
  struct {
    bool left = false;
    bool right = false;
    bool middle = false;
  } mouseButtons;

  explicit Application();
  virtual ~Application();

  virtual void render() = 0;
  virtual void viewChanged();
  virtual void keyPressed(uint32_t);
  virtual void buildCommandBuffers();
  virtual void setupDepthStencil();
  virtual void setupFrameBuffer();
  virtual void setupRenderPass();
  virtual void getEnabledFeatures();
  virtual void prepare();

  void initVulkan(Window *window);
  void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free);
  void loop(Window *window);
  void update_camera(float frame_time);
  void check_tick_finnished(Window *window);
  void parse_arguments(const int argc, const char *argv[]);

  void windowResize();
  void createCommandPool();
  bool checkCommandBuffers();
  void createCommandBuffers();
  void destroyCommandBuffers();
  void createPipelineCache();
  void check_view_update();
  void updateTextOverlay();
  void prepareFrame();
  void submitFrame();
  void submit_text_overlay();
  void init_physical_device();
  void init_debugging();
  void get_physical_device_properties();
  void init_semaphores();
  void list_gpus();

  VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin);
  std::string make_title_string();
};
}
