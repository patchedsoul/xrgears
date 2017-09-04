/*
 * XRGears
 *
 * Copyright (C) 2016 Sascha Willems - www.saschawillems.de
 * Copyright (C) 2017 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 *
 * Based on Vulkan Examples written by Sascha Willems
 */

#pragma once

#include <string>

#include "vksApplication.hpp"

class VikWindow {
 public:
  VikWindow() {}
  ~VikWindow() {}

  virtual int init(vks::Application *app) = 0;

  virtual const char* requiredExtensionName() = 0;
  virtual void initSwapChain(const VkInstance &instance, vks::SwapChain* swapChain) = 0;
  virtual void update_window_title(const std::string& title) = 0;
  virtual void flush(vks::Application *app) = 0;

  virtual void loop(vks::Application *app) {
    while (!app->quit) {
      app->timer.start();
      app->check_view_update();

      flush(app);

      app->render();
      app->timer.increment();
      float frame_time = app->timer.update_frame_time();
      update_camera(app, frame_time);
      app->timer.update_animation_timer();
      check_tick_finnished(app);
    }
  }

  void update_camera(vks::Application *app, float frame_time) {
    app->camera.update(frame_time);
    if (app->camera.moving())
      app->viewUpdated = true;
  }

  void check_tick_finnished(vks::Application *app) {
    if (app->timer.tick_finnished()) {
      app->timer.update_fps();
      if (!app->enableTextOverlay)
        update_window_title(app->getWindowTitle());
      else
        app->updateTextOverlay();
      app->timer.reset();
    }
  }

};
