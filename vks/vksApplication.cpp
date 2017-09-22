/*
* Vulkan Example base class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include <vector>
#include <string>

#include "vksApplication.hpp"
#include "VikAssets.hpp"
#include "vksWindow.hpp"
#include "vksWindowWayland.hpp"

namespace vks {

Application::Application() {
  // Check for a valid asset path
  struct stat info;
  if (stat(VikAssets::getAssetPath().c_str(), &info) != 0)
    vik_log_f("Error: Could not find asset path in %s", VikAssets::getAssetPath().c_str());

  renderer = new Renderer();

  std::function<void()> set_window_resize_cb = [this]() { windowResize(); };
  std::function<void()> enabled_features_cb = [this]() { getEnabledFeatures(); };

  renderer->set_enabled_features_cb(enabled_features_cb);
  renderer->set_window_resize_cb(set_window_resize_cb);
}

Application::~Application() {
  delete renderer;
}

void Application::parse_arguments(const int argc, const char *argv[]) {
  std::vector<const char*> args;
  for (size_t i = 0; i < argc; i++) { args.push_back(argv[i]); };

  // Parse command line arguments
  for (size_t i = 0; i < args.size(); i++) {
    if (args[i] == std::string("-validation"))
      settings.validation = true;
    if (args[i] == std::string("-vsync"))
      settings.vsync = true;
    if (args[i] == std::string("-fullscreen"))
      settings.fullscreen = true;
    if ((args[i] == std::string("-w")) || (args[i] == std::string("-width"))) {
      char* endptr;
      uint32_t w = strtol(args[i + 1], &endptr, 10);
      if (endptr != args[i + 1]) renderer->width = w;
    }
    if ((args[i] == std::string("-g")) || (args[i] == std::string("-gpu"))) {
      char* endptr;
      uint32_t gpu_index = strtol(args[i + 1], &endptr, 10);
      if (endptr != args[i + 1]) settings.gpu_index = gpu_index;
    }
    if ((args[i] == std::string("-h")) || (args[i] == std::string("-height"))) {
      char* endptr;
      uint32_t h = strtol(args[i + 1], &endptr, 10);
      if (endptr != args[i + 1]) renderer->height = h;
    }
    // List available GPUs
    if (args[i] == std::string("-listgpus"))
      settings.list_gpus_and_exit = true;
  }
}

void Application::check_view_update() {
  if (viewUpdated) {
    viewUpdated = false;
    viewChanged();
  }
}

void Application::prepare() {

  window = new vks::WindowWayland();
  //VikWindow * window = new vks::WindowXCB();
  //VikWindow * window = new vks::WindowKMS();
  renderer->initVulkan(&settings, window, name);
  window->init(this);
  window->init_swap_chain(renderer);

  if (renderer->vksDevice->enableDebugMarkers)
    vks::debugmarker::setup(renderer->device);
  renderer->createCommandPool();
  // TODO: create DRM swapchain here

  renderer->swapChain.create(&renderer->width, &renderer->height, settings.vsync);

  renderer->createCommandBuffers();
  renderer->setupDepthStencil();
  renderer->setupRenderPass();
  renderer->createPipelineCache();
  renderer->setupFrameBuffer();

  renderer->init_text_overlay(title);

  vik_log_d("prepare done");
}

void Application::loop() {
  renderer->destWidth = renderer->width;
  renderer->destHeight = renderer->height;

  while (!quit) {
    renderer->timer.start();
    check_view_update();

    window->iter(this);

    render();
    renderer->timer.increment();
    float frame_time = renderer->timer.update_frame_time();
    update_camera(frame_time);
    renderer->timer.update_animation_timer();
    renderer->check_tick_finnished(window, title);
  }

  // Flush device to make sure all resources can be freed
  vkDeviceWaitIdle(renderer->device);
}

void Application::update_camera(float frame_time) {
  camera.update(frame_time);
  if (camera.moving())
    viewUpdated = true;
}

void Application::viewChanged() {}
void Application::keyPressed(uint32_t) {}
void Application::buildCommandBuffers() {}



void Application::getEnabledFeatures() {}

void Application::windowResize() {
  if (!prepared)
    return;
  prepared = false;

  // Ensure all operations on the device have been finished before destroying resources
  vkDeviceWaitIdle(renderer->device);

  // Recreate swap chain
  renderer->width = renderer->destWidth;
  renderer->height = renderer->destHeight;
  // TODO: Create kms swapchain here.

  renderer->swapChain.create(&renderer->width, &renderer->height, settings.vsync);
  // Recreate the frame buffers

  vkDestroyImageView(renderer->device, renderer->depthStencil.view, nullptr);
  vkDestroyImage(renderer->device, renderer->depthStencil.image, nullptr);
  vkFreeMemory(renderer->device, renderer->depthStencil.mem, nullptr);
  renderer->setupDepthStencil();

  for (uint32_t i = 0; i < renderer->frameBuffers.size(); i++)
    vkDestroyFramebuffer(renderer->device, renderer->frameBuffers[i], nullptr);
  renderer->setupFrameBuffer();

  // Command buffers need to be recreated as they may store
  // references to the recreated frame buffer
  renderer->destroyCommandBuffers();
  renderer->createCommandBuffers();
  buildCommandBuffers();

  vkDeviceWaitIdle(renderer->device);

  if (renderer->enableTextOverlay) {
    renderer->textOverlay->reallocateCommandBuffers();
    renderer->updateTextOverlay(title);
  }

  camera.updateAspectRatio((float)renderer->width / (float)renderer->height);

  // Notify derived class
  //windowResized();
  viewChanged();

  prepared = true;
}
}
