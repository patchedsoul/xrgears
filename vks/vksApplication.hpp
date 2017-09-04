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
#include "vksFrameCounter.hpp"


class VikWindow;

namespace vks {

class Application {

  vks::Renderer *renderer;

 public:

  vks::Timer timer;

  PFN_vkGetPhysicalDeviceFeatures2KHR fpGetPhysicalDeviceFeatures2KHR;
  PFN_vkGetPhysicalDeviceProperties2KHR fpGetPhysicalDeviceProperties2KHR;

  void printMultiviewProperties(VkDevice logicalDevice, VkPhysicalDevice physicalDevice);

  // fps timer (one second interval)

  // Get window title with example name, device, et.
  std::string getWindowTitle();
  /** brief Indicates that the view (position, rotation) has changed and */
  bool viewUpdated = false;
  // Destination dimensions for resizing the window
  uint32_t destWidth;
  uint32_t destHeight;
  bool resizing = false;
  // Called if the window is resized and some resources have to be recreatesd
  void windowResize();

 public:
  // Frame counter to display fps

  // Vulkan instance, stores all per-application states
  VkInstance instance;
  // Physical device (GPU) that Vulkan will ise
  VkPhysicalDevice physicalDevice;
  // Stores physical device properties (for e.g. checking device limits)
  VkPhysicalDeviceProperties deviceProperties;
  // Stores the features available on the selected physical device (for e.g. checking if a feature is available)
  VkPhysicalDeviceFeatures deviceFeatures;
  // Stores all available memory (type) properties for the physical device
  VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
  /**
  * Set of physical device features to be enabled for this example (must be set in the derived constructor)
  *
  * @note By default no phyiscal device features are enabled
  */
  VkPhysicalDeviceFeatures enabledFeatures{};
  /** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
  std::vector<const char*> enabledExtensions;
  /** @brief Logical device, application's view of the physical device (GPU) */
  // todo: getter? should always point to VulkanDevice->device
  VkDevice device;
  // Handle to the device graphics queue that command buffers are submitted to
  VkQueue queue;
  // Depth buffer format (selected during Vulkan initialization)
  VkFormat depthFormat;
  // Command buffer pool
  VkCommandPool cmdPool;
  /** @brief Pipeline stages used to wait at for graphics queue submissions */
  VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  // Contains command buffers and semaphores to be presented to the queue
  VkSubmitInfo submitInfo;
  // Command buffers used for rendering
  std::vector<VkCommandBuffer> drawCmdBuffers;
  // Global render pass for frame buffer writes
  VkRenderPass renderPass;
  // List of available frame buffers (same as number of swap chain images)
  std::vector<VkFramebuffer>frameBuffers;
  // Active frame buffer index
  uint32_t currentBuffer = 0;
  // Descriptor set pool
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  // List of shader modules created (stored for cleanup)
  std::vector<VkShaderModule> shaderModules;
  // Pipeline cache object
  VkPipelineCache pipelineCache;
  // Wraps the swap chain to present images (framebuffers) to the windowing system
  SwapChain swapChain;
  // Synchronization semaphores

  struct {
    // Swap chain image presentation
    VkSemaphore presentComplete;
    // Command buffer submission and execution
    VkSemaphore renderComplete;
    // Text overlay submission and execution
    VkSemaphore textOverlayComplete;
  } semaphores;

 public:
  bool prepared = false;
  /*
  uint32_t width = 1080*2;
  uint32_t height = 1200;
  */
  /*
  uint32_t width = 1920;
  uint32_t height = 1200;
*/
  uint32_t width = 2560;
  uint32_t height = 1440;



  /*
  uint32_t width = 1366;
  uint32_t height = 768;
  */



  /** @brief Encapsulated physical and logical vulkan device */
  vks::Device *vulkanDevice;

  /** @brief Example settings that can be changed e.g. by command line arguments */
  struct Settings {
    /** @brief Activates validation layers (and message output) when set to true */
    bool validation = true;
    /** @brief Set to true if fullscreen mode has been requested via command line */
    bool fullscreen = false;
    /** @brief Set to true if v-sync will be forced for the swapchain */
    bool vsync = false;
  } settings;

  VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

  float zoom = 0;

  static std::vector<const char*> args;

  bool paused = false;

  bool enableTextOverlay = true;
  TextOverlay *textOverlay;

  // Use to adjust mouse rotation speed
  float rotationSpeed = 1.0f;
  // Use to adjust mouse zoom speed
  float zoomSpeed = 1.0f;

  Camera camera;

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

  struct {
    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;
  } depthStencil;

  // Gamepad state (only one pad supported)
  struct {
    glm::vec2 axisLeft = glm::vec2(0.0f);
    glm::vec2 axisRight = glm::vec2(0.0f);
  } gamePadState;

  // Default ctor
  explicit Application(bool enableValidation);

  // dtor
  virtual ~Application();

  // Setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
  void initVulkan(VikWindow *window);

  /**
  * Create the application wide Vulkan instance
  *
  * @note Virtual, can be overriden by derived example class for custom instance creation
  */
  virtual VkResult createInstance(bool enableValidation, VikWindow *window);

  // Pure virtual render function (override in derived class)
  virtual void render() = 0;
  // Called when view change occurs
  // Can be overriden in derived class to e.g. update uniform buffers
  // Containing view dependant matrices
  virtual void viewChanged();
  // Called if a key is pressed
  /** @brief (Virtual) Called after a key was pressed, can be used to do custom key handling */
  virtual void keyPressed(uint32_t);
  // Called when the window has been resized
  // Can be overriden in derived class to recreate or rebuild resources attached to the frame buffer / swapchain
  virtual void windowResized();
  // Pure virtual function to be overriden by the dervice class
  // Called in case of an event where e.g. the framebuffer has to be rebuild and thus
  // all command buffers that may reference this
  virtual void buildCommandBuffers();

  // Creates a new (graphics) command pool object storing command buffers
  void createCommandPool();
  // Setup default depth and stencil views
  virtual void setupDepthStencil();
  // Create framebuffers for all requested swap chain images
  // Can be overriden in derived class to setup a custom framebuffer (e.g. for MSAA)
  virtual void setupFrameBuffer();
  // Setup a default render pass
  // Can be overriden in derived class to setup a custom render pass (e.g. for MSAA)
  virtual void setupRenderPass();

  /** @brief (Virtual) Called after the physical device features have been read, can be used to set features to enable on the device */
  virtual void getEnabledFeatures();

  // Connect and prepare the swap chain
  // Create swap chain images
  void setupSwapChain();

  // Check if command buffers are valid (!= VK_NULL_HANDLE)
  bool checkCommandBuffers();
  // Create command buffers for drawing commands
  void createCommandBuffers();
  // Destroy all command buffers and set their handles to VK_NULL_HANDLE
  // May be necessary during runtime if options are toggled
  void destroyCommandBuffers();

  // Command buffer creation
  // Creates and returns a new command buffer
  VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin);
  // End the command buffer, submit it to the queue and free (if requested)
  // Note : Waits for the queue to become idle
  void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free);

  // Create a cache pool for rendering pipelines
  void createPipelineCache();

  // Prepare commonly used Vulkan functions
  virtual void prepare();

  // Start the main render loop
  void renderLoopWrap(VikWindow *window);

  void updateTextOverlay();

  /** @brief (Virtual) Called when the text overlay is updating, can be used to add custom text to the overlay */
  virtual void getOverlayText(TextOverlay*);

  // Prepare the frame for workload submission
  // - Acquires the next image from the swap chain
  // - Sets the default wait and signal semaphores
  void prepareFrame();

  // Submit the frames' workload
  // - Submits the text overlay (if enabled)
  void submitFrame();
};
}
