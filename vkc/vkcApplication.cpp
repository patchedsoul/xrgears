#include <assert.h>

#include "vkcApplication.hpp"

#include "vkcWindowXCB.hpp"
#include "vkcWindowKMS.hpp"
#include "vkcWindowWayland.hpp"
#include "vkcRenderer.hpp"

namespace vkc {

Application::Application(uint32_t w, uint32_t h) {
  renderer = new Renderer(w, h);
}

Application::~Application() {
  delete renderer;
}

void Application::parse_args(int argc, char *argv[]) {
  /* Setting '+' in the optstring is the same as setting POSIXLY_CORRECT in
       * the enviroment. It tells getopt to stop parsing argv when it encounters
       * the first non-option argument; it also prevents getopt from permuting
       * argv during parsing.
       *
       * The initial ':' in the optstring makes getopt return ':' when an option
       * is missing a required argument.
       */
  static const char *optstring = "+:nm:o:";

  int opt;
  while ((opt = getopt(argc, argv, optstring)) != -1) {
    switch (opt) {
      case 'm':
        settings.type = vik::Window::window_type_from_string(optarg);
        if (settings.type == vik::Window::INVALID)
          vik_log_f("option -m given bad display mode");
        break;
      case '?':
        vik_log_f("invalid option '-%c'", optopt);
        break;
      case ':':
        vik_log_f("option -%c requires an argument", optopt);
        break;
      default:
        assert(!"unreachable");
        break;
    }
  }

  if (optind != argc)
    vik_log_w("trailing args");
}


int Application::init_window(vik::Window::window_type m) {
  switch (settings.type) {
    case vik::Window::KMS:
      window = new WindowKMS();
      break;
    case vik::Window::XCB_SIMPLE:
      window = new WindowXCB();
      break;
    case vik::Window::WAYLAND_XDG:
      window = new WindowWayland();
      break;
    case vik::Window::WAYLAND_LEGACY:
      break;
    case vik::Window::XCB_MOUSE:
      break;
    case vik::Window::AUTO:
      return -1;
  }

  std::function<void()> init_cb = std::bind(&Application::init, this);
  std::function<void()> update_cb = std::bind(&Application::update_scene, this);
  std::function<void()> quit_cb = [this]() { quit = true; };

  window->set_init_cb(init_cb);
  window->set_update_cb(update_cb);
  window->set_quit_cb(quit_cb);

  return window->init(renderer);
}

void Application::init_window_auto() {
  settings.type = vik::Window::WAYLAND_XDG;
  if (init_window(settings.type) == -1) {
    vik_log_e("failed to initialize wayland, falling back to xcb");
    delete(window);
    settings.type = vik::Window::XCB_SIMPLE;
    if (init_window(settings.type) == -1) {
      vik_log_e("failed to initialize xcb, falling back to kms");
      delete(window);
      settings.type = vik::Window::KMS;
      init_window(settings.type);
    }
  }
}

void Application::init_window() {
  if (settings.type == vik::Window::AUTO)
    init_window_auto();
  else if (init_window(settings.type) == -1)
    vik_log_f("failed to initialize %s", window->name.c_str());
}

void Application::loop() {
  while (!quit)
    window->iter(renderer);
}
}

