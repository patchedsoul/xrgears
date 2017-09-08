#include <assert.h>

#include "vkcApplication.hpp"

#include "vkcWindowXCB.hpp"
#include "vkcWindowKMS.hpp"
#include "vkcWindowWayland.hpp"
#include "vkcRenderer.hpp"

namespace vkc {

Application::Application(uint32_t w, uint32_t h) {
  mode = AUTO;
  renderer = new Renderer(w, h);
}

Application::~Application() {
  delete renderer;
}

bool Application::display_mode_from_string(const char *s) {
  if (streq(s, "auto")) {
    mode = AUTO;
    return true;
  } else if (streq(s, "kms")) {
    mode = KMS;
    return true;
  } else if (streq(s, "xcb")) {
    mode = XCB;
    return true;
  } else if (streq(s, "wayland")) {
    mode = WAYLAND;
    return true;
  } else {
    return false;
  }
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
        if (!display_mode_from_string(optarg))
          vik_log_e("option -m given bad display mode");
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


int Application::init_display_mode(window_type m) {
  switch (mode) {
    case KMS:
      display = new WindowKMS();
      break;
    case XCB:
      display = new WindowXCB();
      break;
    case WAYLAND:
      display = new WindowWayland();
      break;
    case AUTO:
      return -1;
  }

  std::function<void()> init_cb = std::bind(&Application::init, this);
  std::function<void()> update_cb = std::bind(&Application::update_scene, this);

  display->set_init_cb(init_cb);
  display->set_update_cb(update_cb);

  return display->init(renderer);
}

void Application::init_display_mode_auto() {
  mode = WAYLAND;
  if (init_display_mode(mode) == -1) {
    vik_log_e("failed to initialize wayland, falling back to xcb");
    delete(display);
    mode = XCB;
    if (init_display_mode(mode) == -1) {
      vik_log_e("failed to initialize xcb, falling back to kms");
      delete(display);
      mode = KMS;
      init_display_mode(mode);
    }
  }
}

void Application::init_display() {
  if (mode == AUTO)
    init_display_mode_auto();
  else if (init_display_mode(mode) == -1)
    vik_log_f("failed to initialize %s", display->name.c_str());
}

void Application::loop() {
  display->loop(renderer);
}
}

