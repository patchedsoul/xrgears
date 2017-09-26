#pragma once

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <functional>

#include "vikApplication.hpp"


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

    std::function<void(uint32_t width, uint32_t height)> dimension_cb =
        [this](uint32_t width, uint32_t height) {
      renderer->width = width;
      renderer->height = height;
    };

    window->set_dimension_cb(dimension_cb);

    window->set_update_cb(update_cb);
    window->set_quit_cb(quit_cb);

    std::function<void(Input::Key key, bool state)> keyboard_key_cb =
        [this](Input::Key key, bool state) {
      switch (key) {
        case Input::Key::ESCAPE:
          quit = true;
          break;
      }
    };
    window->set_keyboard_key_cb(keyboard_key_cb);

    window->init(renderer->width, renderer->height, settings.fullscreen);

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

  void init_window_auto() {
    settings.type = Settings::WAYLAND_XDG;
    if (init_window_from_settings() == -1) {
      vik_log_e("failed to initialize wayland, falling back to xcb");
      delete(window);
      settings.type = Settings::XCB_SIMPLE;
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

  void loop() {
    while (!quit)
      window->iterate(renderer);
  }

  virtual void init_cb() = 0;
  virtual void update_scene() = 0;

};
}