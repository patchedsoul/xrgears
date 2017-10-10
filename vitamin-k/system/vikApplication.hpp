/*
 * vitamin-k
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <sys/stat.h>

#include <vulkan/vulkan.h>

#include <iostream>
#include <chrono>
#include <string>
#include <array>
#include <vector>

#include "vikSettings.hpp"

#include "../window/vikWindowXCB.hpp"
#include "../window/vikWindowWaylandXDG.hpp"
#include "../window/vikWindowWaylandShell.hpp"
#include "../window/vikWindowKMS.hpp"
#include "../window/vikWindowKhrDisplay.hpp"

#include "../render/vikTools.hpp"
#include "../render/vikInitializers.hpp"
#include "../scene/vikCamera.hpp"
#include "../render/vikRendererTextOverlay.hpp"
#include "../render/vikTimer.hpp"
#include "../input/vikHMD.hpp"


namespace vik {
class Application {
 public:
  Settings settings;
  Window *window;
  bool quit = false;

  RendererTextOverlay *renderer = nullptr;
  Camera *camera = nullptr;

  bool viewUpdated = false;

  std::string name = "vitamin-k Example";


  Application(int argc, char *argv[]) {
    if (!settings.parse_args(argc, argv))
      vik_log_f("Invalid arguments.");

    if (settings.list_hmds_and_exit) {
       HMD::enumerate_hmds();
       exit(0);
    }

    renderer = new RendererTextOverlay(&settings);

    init_window();

    auto set_window_resize_cb = [this]() { resize(); };
    renderer->set_window_resize_cb(set_window_resize_cb);

    auto enabled_features_cb = [this]() { enable_required_features(); };
    renderer->set_enabled_features_cb(enabled_features_cb);

    auto frame_start_cb = [this]() {
      check_view_update();
    };
    renderer->set_frame_start_cb(frame_start_cb);

    auto frame_end_cb = [this](float frame_time) {
      update_camera(frame_time);
    };
    renderer->set_frame_end_cb(frame_end_cb);

    auto render_cb = [this]() {
      render();
    };
    renderer->set_render_cb(render_cb);

    if (camera != nullptr)
      camera->set_view_updated_cb([this]() { viewUpdated = true; });
  }

  ~Application()  {
    if (renderer)
      delete renderer;
  }

  void set_window_callbacks() {
    window->set_pointer_motion_cb(
          [this](double x, double y) {
      camera->pointer_motion_cb(x,y);
    });
    window->set_pointer_button_cb(
          [this](Input::MouseButton button, bool state) {
      camera->pointer_button_cb(button, state);
    });
    window->set_pointer_axis_cb(
          [this](Input::MouseScrollAxis axis, double value) {
      camera->pointer_axis_cb(axis, value);
    });

    auto keyboard_key_cb = [this](Input::Key key, bool state) {
      switch (key) {
        case Input::Key::P:
          if (state)
            renderer->timer.toggle_animation_pause();
          break;
        case Input::Key::F1:
          if (state && settings.enable_text_overlay)
            renderer->textOverlay->visible = !renderer->textOverlay->visible;
          break;
        case Input::Key::ESCAPE:
          quit = true;
          break;
        default:
          break;
      }

      if (state)
        key_pressed(key);

      camera->keyboard_key_cb(key, state);
    };
    window->set_keyboard_key_cb(keyboard_key_cb);

    auto quit_cb = [this]() { quit = true; };
    window->set_quit_cb(quit_cb);
  }

  int set_and_init_window() {
    set_window_callbacks();
    renderer->set_window(window);
    return window->init();
  }

  int init_window_from_settings() {
    switch (settings.type) {
      case Settings::KMS:
        window = new WindowKMS(&settings);
        return set_and_init_window();
      case Settings::XCB:
        window = new WindowXCB(&settings);
        return set_and_init_window();
      case Settings::WAYLAND_XDG:
        window = new WindowWaylandXDG(&settings);
        return set_and_init_window();
      case Settings::WAYLAND_SHELL:
        window = new WindowWaylandShell(&settings);
        return set_and_init_window();
      case Settings::KHR_DISPLAY:
        window = new WindowKhrDisplay(&settings);
        return set_and_init_window();
      default:
        return -1;
    }
    return 0;
  }

  void init_window_auto() {
    settings.type = Settings::WAYLAND_XDG;
    if (init_window_from_settings() == -1) {
      vik_log_w("Failed to initialize wayland-xdg, falling back to xcb.");
      delete(window);
      settings.type = Settings::XCB;
      if (init_window_from_settings() == -1) {
        vik_log_w("Failed to initialize xcb, falling back to kms.");
        delete(window);
        settings.type = Settings::KMS;
        init_window_from_settings();
      }
    }
  }

  void init_window() {
    if (settings.type == Settings::AUTO)
      init_window_auto();
    else if (init_window_from_settings() == -1)
      vik_log_f("Failed to initialize %s back end.", window->name.c_str());
  }

  virtual void render() = 0;
  virtual void view_changed_cb() {}
  virtual void key_pressed(Input::Key) {}
  virtual void build_command_buffers() {}
  virtual void enable_required_features() {}
  virtual void update_text_overlay(vik::TextOverlay *overlay) {}

  void check_view_update() {
    if (viewUpdated) {
      viewUpdated = false;
      view_changed_cb();
    }
  }

  virtual void init() {
    renderer->init(name);

    std::function<void(vik::TextOverlay *overlay)>
      text_overlay_update_cb = [this](vik::TextOverlay *overlay) {
      update_text_overlay(overlay);
    };

    renderer->textOverlay->update_cb = text_overlay_update_cb;

  }

  void loop() {
    while (!quit)
      renderer->render();
    renderer->wait_idle();
  }

  void update_camera(float frame_time) {
    camera->update_movement(frame_time);
    if (camera->moving())
      viewUpdated = true;
  }

  void resize() {
    build_command_buffers();

    renderer->wait_idle();

    camera->update_aspect_ratio(renderer->get_aspect_ratio());

    view_changed_cb();
  }
};
}  // namespace vik
