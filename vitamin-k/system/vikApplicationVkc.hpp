#pragma once

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <functional>

#include "vikApplication.hpp"

#include "window/vikWindowXCBSimple.hpp"
#include "window/vikWindowKMS.hpp"
#include "window/vikWindowWaylandXDG.hpp"
#include "render/vikRendererVkc.hpp"

namespace vik {

class ApplicationVkc : public Application {
public:
  RendererVkc *renderer;


  ApplicationVkc(uint32_t w, uint32_t h) {
    renderer = new RendererVkc(w, h);
  }

  ~ApplicationVkc() {
    delete renderer;
  }

  void parse_args(int argc, char *argv[]) {
    if (!settings.parse_args(argc, argv))
      vik_log_f("Invalid arguments.");
    renderer->set_settings(&settings);
  }

  void init() {
    init_window();

    std::function<void()> update_cb = [this]() { update_scene(); };
    std::function<void()> quit_cb = [this]() { quit = true; };

    window->set_update_cb(update_cb);
    window->set_quit_cb(quit_cb);
    window->init(renderer);

    renderer->init_vulkan("vkcube", window->required_extensions());

    if (!window->check_support(renderer->physical_device))
      vik_log_f("Vulkan not supported on given surface");

    window->init_swap_chain(renderer);

    std::function<void(uint32_t index)> render_cb =
        [this](uint32_t index) { renderer->render(index); };
    renderer->swap_chain->set_render_cb(render_cb);

    init_cb();

    renderer->create_frame_buffers(renderer->swap_chain);
  }

  int init_window_from_settings() {
    switch (settings.type) {
      case Window::KMS:
        window = new WindowKMS();
        break;
      case Window::XCB_SIMPLE:
        window = new WindowXCBSimple();
        break;
      case Window::WAYLAND_XDG:
        window = new WindowWaylandXDG();
        break;
      case Window::WAYLAND_LEGACY:
        break;
      case Window::XCB_MOUSE:
        break;
      case Window::AUTO:
        return -1;
    }
  }

  void init_window_auto() {
    settings.type = Window::WAYLAND_XDG;
    if (init_window_from_settings() == -1) {
      vik_log_e("failed to initialize wayland, falling back to xcb");
      delete(window);
      settings.type = Window::XCB_SIMPLE;
      if (init_window_from_settings() == -1) {
        vik_log_e("failed to initialize xcb, falling back to kms");
        delete(window);
        settings.type = Window::KMS;
        init_window_from_settings();
      }
    }
  }

  void init_window() {
    if (settings.type == Window::AUTO)
      init_window_auto();
    else if (init_window_from_settings() == -1)
      vik_log_f("failed to initialize %s", window->name.c_str());
  }

  void loop() {
    while (!quit)
      window->iterate(renderer);
  }

  virtual void init_cb() = 0;
  virtual void update_scene() = 0;

};
}
