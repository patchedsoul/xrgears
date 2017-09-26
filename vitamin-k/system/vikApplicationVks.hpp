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
#include "render/vikRendererVks.hpp"
#include "render/vikTimer.hpp"

#include "window/vikWindowWaylandShell.hpp"
#include "window/vikWindowXCBInput.hpp"
#include "window/vikWindowKhrDisplay.hpp"

#include "vikApplication.hpp"

namespace vik {
class Window;

class ApplicationVks : public vik::Application {

public:
  RendererVks *renderer;
  CameraBase camera;

  bool prepared = false;
  bool viewUpdated = false;
  bool resizing = false;

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

  ApplicationVks() {
    renderer = new RendererVks();

    std::function<void()> set_window_resize_cb = [this]() { windowResize(); };
    renderer->set_window_resize_cb(set_window_resize_cb);

    std::function<void()> enabled_features_cb = [this]() { getEnabledFeatures(); };
    renderer->set_enabled_features_cb(enabled_features_cb);
  }

  ~ApplicationVks() {
    delete renderer;
  }

  virtual void render() = 0;
  virtual void viewChanged() {}
  virtual void keyPressed(uint32_t) {}
  virtual void buildCommandBuffers() {}
  virtual void getEnabledFeatures() {}

  void parse_arguments(int argc, char *argv[]) {
    settings.parse_args(argc, argv);
    renderer->set_settings(&settings);
  }

  void check_view_update() {
    if (viewUpdated) {
      viewUpdated = false;
      viewChanged();
    }
  }

  virtual void prepare() {

    switch (settings.type) {
      case vik::Window::WAYLAND_LEGACY:
        window = new vik::WindowWaylandShell();
        break;
      case vik::Window::XCB_MOUSE:
        window = new vik::WindowXCBInput();
        break;
      case vik::Window::KHR_DISPLAY:
        window = new vik::WindowKhrDisplay();
        break;
      default:
        vik_log_f("Unsupported window backend.");
    }

    std::function<void(double x, double y)> pointer_motion_cb =
        [this](double x, double y) {
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

    std::function<void(vik::Input::MouseButton button, bool state)> pointer_button_cb =
        [this](vik::Input::MouseButton button, bool state) {
      switch (button) {
        case vik::Input::MouseButton::Left:
          mouseButtons.left = state;
          break;
        case vik::Input::MouseButton::Middle:
          mouseButtons.middle = state;
          break;
        case vik::Input::MouseButton::Right:
          mouseButtons.right = state;
          break;
      }
    };

    std::function<void(vik::Input::MouseScrollAxis axis, double value)> pointer_axis_cb =
        [this](vik::Input::MouseScrollAxis axis, double value) {
      switch (axis) {
        case vik::Input::MouseScrollAxis::X:
          zoom += value * 0.005f * zoomSpeed;
          camera.translate(glm::vec3(0.0f, 0.0f, value * 0.005f * zoomSpeed));
          viewUpdated = true;
          break;
        default:
          break;
      }
    };

    std::function<void(vik::Input::Key key, bool state)> keyboard_key_cb =
        [this](vik::Input::Key key, bool state) {
      switch (key) {
        case vik::Input::Key::W:
          camera.keys.up = state;
          break;
        case vik::Input::Key::S:
          camera.keys.down = state;
          break;
        case vik::Input::Key::A:
          camera.keys.left = state;
          break;
        case vik::Input::Key::D:
          camera.keys.right = state;
          break;
        case vik::Input::Key::P:
          if (state)
            renderer->timer.toggle_animation_pause();
          break;
        case vik::Input::Key::F1:
          if (state && renderer->enableTextOverlay)
            renderer->textOverlay->visible = !renderer->textOverlay->visible;
          break;
        case vik::Input::Key::ESCAPE:
          quit = true;
          break;
      }

      if (state)
        keyPressed(key);
    };

    std::function<void(uint32_t width, uint32_t height)> configure_cb =
        [this](uint32_t width, uint32_t height) {
      if ((prepared) && ((width != renderer->width) || (height != renderer->height))) {
        renderer->destWidth = width;
        renderer->destHeight = height;
        if ((renderer->destWidth > 0) && (renderer->destHeight > 0))
          windowResize();
      }
    };

    std::function<void(uint32_t width, uint32_t height)> dimension_cb =
        [this](uint32_t width, uint32_t height) {
      renderer->width = renderer->destWidth = width;
      renderer->height = renderer->destHeight = height;
    };

    window->set_pointer_motion_cb(pointer_motion_cb);
    window->set_pointer_button_cb(pointer_button_cb);
    window->set_pointer_axis_cb(pointer_axis_cb);
    window->set_keyboard_key_cb(keyboard_key_cb);
    window->set_configure_cb(configure_cb);
    window->set_dimension_cb(dimension_cb);

    std::function<void()> quit_cb = [this]() { quit = true; };
    window->set_quit_cb(quit_cb);

    renderer->init_vulkan(name, window->required_extensions());
    window->init(renderer);

    std::string windowTitle = renderer->make_title_string(title);
    window->update_window_title(windowTitle);

    window->init_swap_chain(renderer);
    renderer->prepare();

    if (renderer->enableTextOverlay)
      renderer->updateTextOverlay(title);

    vik_log_d("prepare done");
  }

  void loop() {
    renderer->destWidth = renderer->width;
    renderer->destHeight = renderer->height;

    while (!quit) {
      renderer->timer.start();
      check_view_update();

      window->iterate(renderer);

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

  void update_camera(float frame_time) {
    camera.update(frame_time);
    if (camera.moving())
      viewUpdated = true;
  }

  void windowResize() {
    if (!prepared)
      return;
    prepared = false;

    renderer->resize();

    buildCommandBuffers();

    vkDeviceWaitIdle(renderer->device);

    if (renderer->enableTextOverlay) {
      renderer->textOverlay->reallocateCommandBuffers();
      renderer->updateTextOverlay(title);
    }

    camera.updateAspectRatio(renderer->get_aspect_ratio());

    // Notify derived class
    //windowResized();
    viewChanged();

    prepared = true;
  }


};
}
