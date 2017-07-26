#pragma once

#include <vulkan/vulkan.h>

#include "vksApplication.hpp"

class ApplicationDisplay  : public Application {
public:
  ApplicationDisplay() {

  }

  ~ApplicationDisplay() {

  }

  void directDisplayRenderLoop() {

  }

  const char* requiredExtensionName() {
    return VK_KHR_DISPLAY_EXTENSION_NAME;
  }

  void initSwapChain() {
      swapChain.initSurface(width, height);
  }

  void renderLoop() {
    while (!quit)
    {
      auto tStart = std::chrono::high_resolution_clock::now();
      if (viewUpdated)
      {
        viewUpdated = false;
        viewChanged();
      }
      render();
      frameCounter++;
      auto tEnd = std::chrono::high_resolution_clock::now();
      auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
      frameTimer = tDiff / 1000.0f;
      camera.update(frameTimer);
      if (camera.moving())
      {
        viewUpdated = true;
      }
      // Convert to clamped timer value
      if (!paused)
      {
        timer += timerSpeed * frameTimer;
        if (timer > 1.0)
        {
          timer -= 1.0f;
        }
      }
      fpsTimer += (float)tDiff;
      if (fpsTimer > 1000.0f)
      {
        lastFPS = frameCounter;
        updateTextOverlay();
        fpsTimer = 0.0f;
        frameCounter = 0;
      }
    }
  }

};
