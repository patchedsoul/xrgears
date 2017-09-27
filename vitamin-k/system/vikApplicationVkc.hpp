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
    init_window();
    renderer = new RendererVkc(&settings, window);

    window->set_update_cb([this]() { update_scene(); });
    window->set_quit_cb([this]() { quit = true; });

    std::function<void(Input::Key key, bool state)> keyboard_key_cb =
        [this](Input::Key key, bool state) {
      switch (key) {
        case Input::Key::ESCAPE:
          quit = true;
          break;
      }
    };
    window->set_keyboard_key_cb(keyboard_key_cb);
  }

  ~ApplicationVkc() {
    delete renderer;
  }

  void init() {
    renderer->init("vkcube");
    init_cb();
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
      renderer->iterate();
  }

  virtual void init_cb() = 0;
  virtual void update_scene() = 0;

};
}
