#pragma once

#include "vikRendererVks.hpp"

namespace vik {

class RendererTextOverlay : public RendererVks {
public:

  TextOverlay *textOverlay;

  RendererTextOverlay(Settings *s, Window *w) : RendererVks(s, w) {}
  ~RendererTextOverlay() {
    if (settings->enable_text_overlay)
      delete textOverlay;
  }

    void init(const std::string &name, const std::string &title) {
      RendererVks::init(name, title);
      if (settings->enable_text_overlay) {
        init_text_overlay();
        update_text_overlay(title);
      }
    }

    void init_text_overlay() {
      // Load the text rendering shaders
      std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
      shaderStages.push_back(Shader::load(device, "base/textoverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
      shaderStages.push_back(Shader::load(device, "base/textoverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));

      textOverlay = new TextOverlay(
            vksDevice,
            queue,
            &frame_buffers,
            window->get_swap_chain()->surface_format.format,
            depthFormat,
            &width,
            &height,
            shaderStages);
    }

    VkSubmitInfo init_text_submit_info() {
      // Wait for color attachment output to finish before rendering the text overlay
      VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

      VkSubmitInfo submit_info = initializers::submitInfo();
      submit_info.pWaitDstStageMask = &stageFlags;
      submit_info.waitSemaphoreCount = 1;
      submit_info.pWaitSemaphores = &semaphores.render_complete;
      submit_info.signalSemaphoreCount = 1;
      submit_info.pSignalSemaphores = &semaphores.text_overlay_complete;
      submit_info.commandBufferCount = 1;
      return submit_info;
    }

    void update_text_overlay(const std::string& title) {
      if (!settings->enable_text_overlay)
        return;

      std::stringstream ss;
      ss << std::fixed
         << std::setprecision(3)
         << (timer.frame_time_seconds * 1000.0f)
         << "ms (" << timer.frames_per_second
         << " fps)";
      std::string deviceName(deviceProperties.deviceName);

      textOverlay->update(title, ss.str(), deviceName);
    }

    VkSubmitInfo submit_text_overlay() {
      VkSubmitInfo submit_info = init_text_submit_info();
      submit_info.pCommandBuffers = &textOverlay->cmdBuffers[currentBuffer];
      vik_log_check(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
    }

    void check_tick_finnished(const std::string& title) {
      if (timer.tick_finnished()) {
        timer.update_fps();
        if (settings->enable_text_overlay)
          update_text_overlay(title);
        timer.reset();
      }
    }

    void submit_frame() {
      VkSemaphore waitSemaphore;
      if (settings->enable_text_overlay && textOverlay->visible) {
        submit_text_overlay();
        waitSemaphore = semaphores.text_overlay_complete;
      } else {
        waitSemaphore = semaphores.render_complete;
      }

      SwapChainVK *sc = (SwapChainVK*) window->get_swap_chain();
      vik_log_check(sc->present(queue, currentBuffer, waitSemaphore));
      vik_log_check(vkQueueWaitIdle(queue));
    }
};
}
