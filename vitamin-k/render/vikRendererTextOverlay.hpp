/*
 * vitamin-k
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 *
 * Based on Vulkan Examples written by Sascha Willems
 */

#pragma once

#include <string>
#include <vector>

#include "vikRenderer.hpp"

namespace vik {

class RendererTextOverlay : public Renderer {
 public:
  TextOverlay *text_overlay;

  VkSemaphore text_overlay_complete;

  std::string name;

  explicit RendererTextOverlay(Settings *s) : Renderer(s) {}
  ~RendererTextOverlay() {
    vkDestroySemaphore(device, text_overlay_complete, nullptr);
    delete text_overlay;
  }

  void init(const std::string &n,
            std::function<void(vik::TextOverlay *overlay)> cb) {
    Renderer::init(n);
    name = n;
    if (settings->enable_text_overlay) {
      init_text_overlay(cb);
      update_text_overlay();
    }
  }

  void init_text_overlay(std::function<void(vik::TextOverlay *overlay)> cb) {
    // Load the text rendering shaders
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    shaderStages.push_back(Shader::load(device, "base/textoverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
    shaderStages.push_back(Shader::load(device, "base/textoverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));

    text_overlay = new TextOverlay(
          vik_device,
          queue,
          &frame_buffers,
          window->get_swap_chain()->surface_format.format,
          depth_format,
          &width,
          &height,
          shaderStages);
    text_overlay->set_update_cb(cb);
  }

  void update_text_overlay() {
    if (!settings->enable_text_overlay)
      return;

    std::stringstream ss;
    ss << std::fixed
       << std::setprecision(3)
       << (timer.frame_time_seconds * 1000.0f)
       << "ms (" << timer.frames_per_second
       << " fps)";
    std::string deviceName(device_properties.deviceName);

    text_overlay->update(name, ss.str(), deviceName);
  }

  void submit_text_overlay() {
    VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &semaphores.render_complete,
      .pWaitDstStageMask = &stageFlags,
      .commandBufferCount = 1,
      .pCommandBuffers = &text_overlay->cmdBuffers[current_buffer],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &text_overlay_complete
    };

    vik_log_check(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
  }

  void check_tick_finnished() {
    if (timer.tick_finnished()) {
      timer.update_fps();
      if (settings->enable_text_overlay)
        update_text_overlay();
      timer.reset();
    }
  }

  void resize() {
    Renderer::resize();
    if (settings->enable_text_overlay) {
      text_overlay->reallocateCommandBuffers();
      update_text_overlay();
    }
  }

  void submit_frame() {
    VkSemaphore waitSemaphore;
    if (settings->enable_text_overlay && text_overlay->visible) {
      submit_text_overlay();
      waitSemaphore = text_overlay_complete;
    } else {
      waitSemaphore = semaphores.render_complete;
    }

    SwapChainVK *sc = (SwapChainVK*) window->get_swap_chain();
    vik_log_check(sc->present(queue, current_buffer, waitSemaphore));
    vik_log_check(vkQueueWaitIdle(queue));
  }

  void init_semaphores() {
    Renderer::init_semaphores();
    VkSemaphoreCreateInfo semaphore_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    // Create a semaphore used to synchronize command submission
    // Ensures that the image is not presented until all commands for the text overlay have been sumbitted and executed
    // Will be inserted after the render complete semaphore if the text overlay is enabled
    vik_log_check(vkCreateSemaphore(device, &semaphore_info, nullptr, &text_overlay_complete));
  }
};
}  // namespace vik
