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

  ApplicationVkc(int argc, char *argv[]) : Application(argc, argv) {
    renderer = new RendererVkc(&settings);
  }

  ~ApplicationVkc() {
    delete renderer;
  }

  void init() {
    init_window();

    std::function<void()> update_cb = [this]() { update_scene(); };
    std::function<void()> quit_cb = [this]() { quit = true; };

    std::function<void(SwapChain *sc)> recreate_swap_chain_vk_cb = [this](SwapChain *sc) {
      renderer->create_frame_buffers(sc);
    };
    window->set_recreate_swap_chain_vk_cb(recreate_swap_chain_vk_cb);

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

    window->init_swap_chain(renderer->instance, renderer->physical_device,
                            renderer->device, renderer->width, renderer->height);
    renderer->set_swap_chain(window->get_swap_chain());

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
      window->iterate(renderer->queue, renderer->semaphore);
  }

  virtual void init_cb() = 0;
  virtual void update_scene() = 0;

};
}
