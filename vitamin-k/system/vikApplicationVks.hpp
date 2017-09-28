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

#include "render/vikTools.hpp"
#include "render/vikInitializers.hpp"

#include "scene/vikCameraBase.hpp"
#include "render/vikRendererTextOverlay.hpp"

#include "render/vikTimer.hpp"

#include "vikApplication.hpp"

namespace vik {
class Window;

class ApplicationVks : public Application {

public:
  RendererTextOverlay *renderer;
  CameraBase camera;

  bool prepared = false;
  bool viewUpdated = false;

  float zoom = 0;
  float rotationSpeed = 1.0f;
  float zoomSpeed = 1.0f;

  glm::vec3 rotation = glm::vec3();
  glm::vec3 cameraPos = glm::vec3();
  glm::vec2 mousePos;

  std::string title = "Vulkan Example";
  std::string name = "vulkanExample";

  struct {
    bool left = false;
    bool right = false;
    bool middle = false;
  } mouseButtons;

  ApplicationVks(int argc, char *argv[]) : Application(argc, argv) {
    init_window_from_settings();
    renderer = new RendererTextOverlay(&settings, window);

    std::function<void()> set_window_resize_cb = [this]() { resize(); };
    renderer->set_window_resize_cb(set_window_resize_cb);

    std::function<void()> enabled_features_cb = [this]() { get_enabled_features(); };
    renderer->set_enabled_features_cb(enabled_features_cb);

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

    // TODO: move to renderer
    auto configure_cb = [this](uint32_t width, uint32_t height) {
      if ((prepared) && ((width != renderer->width) || (height != renderer->height))) {
        renderer->destWidth = width;
        renderer->destHeight = height;
        if ((renderer->destWidth > 0) && (renderer->destHeight > 0))
          resize();
      }
    };
    window->set_configure_cb(configure_cb);

  }

  ~ApplicationVks() {
    delete renderer;
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
    renderer->init(name, title);
  }

  void loop() {
    while (!quit) {
      renderer->timer.start();
      check_view_update();

      window->iterate(nullptr, nullptr);

      render();
      renderer->timer.increment();
      float frame_time = renderer->timer.update_frame_time();
      update_camera(frame_time);
      renderer->timer.update_animation_timer();
      renderer->check_tick_finnished(title);
    }

    renderer->wait_idle();
  }

  void update_camera(float frame_time) {
    camera.update(frame_time);
    if (camera.moving())
      viewUpdated = true;
  }

  void resize() {
    if (!prepared)
      return;
    prepared = false;

    renderer->resize();

    build_command_buffers();

    vkDeviceWaitIdle(renderer->device);

    if (settings.enable_text_overlay) {
      renderer->textOverlay->reallocateCommandBuffers();
      renderer->update_text_overlay(title);
    }

    camera.update_aspect_ratio(renderer->get_aspect_ratio());

    // Notify derived class
    //windowResized();
    view_changed_cb();

    prepared = true;
  }


};
}
