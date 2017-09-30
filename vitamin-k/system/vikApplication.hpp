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

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

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

#include "../scene/vikCameraBase.hpp"
#include "../render/vikRendererTextOverlay.hpp"

#include "../render/vikTimer.hpp"


namespace vik {
class Application {
 public:
  Settings settings;
  Window *window;
  bool quit = false;

  RendererTextOverlay *renderer;
  CameraBase camera;

  bool viewUpdated = false;

  float zoom = 0;
  float rotationSpeed = 1.0f;
  float zoomSpeed = 1.0f;

  glm::vec3 rotation = glm::vec3();
  glm::vec3 cameraPos = glm::vec3();
  glm::vec2 mousePos;

  std::string name = "vitamin-k Example";

  struct {
    bool left = false;
    bool right = false;
    bool middle = false;
  } mouseButtons;

  Application(int argc, char *argv[]) {
    if (!settings.parse_args(argc, argv))
      vik_log_f("Invalid arguments.");
    init_window();
    renderer = new RendererTextOverlay(&settings, window);

    auto set_window_resize_cb = [this]() { resize(); };
    renderer->set_window_resize_cb(set_window_resize_cb);

    auto enabled_features_cb = [this]() { get_enabled_features(); };
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

    auto pointer_motion_cb = [this](double x, double y) {
      double dx = mousePos.x - x;
      double dy = mousePos.y - y;

      if (mouseButtons.left) {
        rotation.x += dy * 1.25f * rotationSpeed;
        rotation.y -= dx * 1.25f * rotationSpeed;
        camera.rotate(glm::vec3(
                        dy * camera.rotationSpeed,
                        -dx * camera.rotationSpeed,
                        0.0f));
        viewUpdated = true;
      }

      if (mouseButtons.right) {
        zoom += dy * .005f * zoomSpeed;
        camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f * zoomSpeed));
        viewUpdated = true;
      }

      if (mouseButtons.middle) {
        cameraPos.x -= dx * 0.01f;
        cameraPos.y -= dy * 0.01f;
        camera.translate(glm::vec3(-dx * 0.01f, -dy * 0.01f, 0.0f));
        viewUpdated = true;
      }
      mousePos = glm::vec2(x, y);
    };

    auto pointer_button_cb = [this](Input::MouseButton button, bool state) {
      switch (button) {
        case Input::MouseButton::Left:
          mouseButtons.left = state;
          break;
        case Input::MouseButton::Middle:
          mouseButtons.middle = state;
          break;
        case Input::MouseButton::Right:
          mouseButtons.right = state;
          break;
      }
    };

    auto pointer_axis_cb = [this](Input::MouseScrollAxis axis, double value) {
      switch (axis) {
        case Input::MouseScrollAxis::X:
          zoom += value * 0.005f * zoomSpeed;
          camera.translate(glm::vec3(0.0f, 0.0f, value * 0.005f * zoomSpeed));
          viewUpdated = true;
          break;
        default:
          break;
      }
    };

    auto keyboard_key_cb = [this](Input::Key key, bool state) {
      switch (key) {
        case Input::Key::W:
          camera.keys.up = state;
          break;
        case Input::Key::S:
          camera.keys.down = state;
          break;
        case Input::Key::A:
          camera.keys.left = state;
          break;
        case Input::Key::D:
          camera.keys.right = state;
          break;
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
      }

      if (state)
        key_pressed(key);
    };

    window->set_pointer_motion_cb(pointer_motion_cb);
    window->set_pointer_button_cb(pointer_button_cb);
    window->set_pointer_axis_cb(pointer_axis_cb);
    window->set_keyboard_key_cb(keyboard_key_cb);

    auto quit_cb = [this]() { quit = true; };
    window->set_quit_cb(quit_cb);
  }

  ~Application()  {
    delete renderer;
  }

  int init_window_from_settings() {
    switch (settings.type) {
      case Settings::KMS:
        window = new WindowKMS(&settings);
        break;
      case Settings::XCB:
        window = new WindowXCB(&settings);
        break;
      case Settings::WAYLAND_XDG:
        window = new WindowWaylandXDG(&settings);
        break;
      case Settings::WAYLAND_SHELL:
        window = new WindowWaylandShell(&settings);
        break;
        break;
      case Settings::KHR_DISPLAY:
        window = new WindowKhrDisplay(&settings);
      case Settings::AUTO:
        return -1;
    }
  }

  void init_window_auto() {
    settings.type = Settings::WAYLAND_XDG;
    if (init_window_from_settings() == -1) {
      vik_log_e("failed to initialize wayland, falling back to xcb");
      delete(window);
      settings.type = Settings::XCB;
      if (init_window_from_settings() == -1) {
        vik_log_e("failed to initialize xcb, falling back to kms");
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
      vik_log_f("failed to initialize %s", window->name.c_str());
  }

  virtual void render() = 0;
  virtual void view_changed_cb() {}
  virtual void key_pressed(uint32_t) {}
  virtual void build_command_buffers() {}
  virtual void get_enabled_features() {}

  void check_view_update() {
    if (viewUpdated) {
      viewUpdated = false;
      view_changed_cb();
    }
  }

  virtual void init() {
    renderer->init(name);
  }

  void loop() {
    while (!quit)
      renderer->render();
    renderer->wait_idle();
  }

  void update_camera(float frame_time) {
    camera.update(frame_time);
    if (camera.moving())
      viewUpdated = true;
  }

  void resize() {
    build_command_buffers();

    renderer->wait_idle();

    camera.update_aspect_ratio(renderer->get_aspect_ratio());

    view_changed_cb();
  }
};
}
