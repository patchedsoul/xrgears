#include <assert.h>

#include "vkcApplication.hpp"

#include "vkcCube.hpp"
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
  bool found_arg_headless = false;
  bool found_arg_display_mode = false;

  while ((opt = getopt(argc, argv, optstring)) != -1) {
    switch (opt) {
      case 'm':
        found_arg_display_mode = true;
        if (!display_mode_from_string(optarg))
          printf("option -m given bad display mode\n");
        break;
      case '?':
        printf("invalid option '-%c'\n", optopt);
        break;
      case ':':
        printf("option -%c requires an argument\n", optopt);
        break;
      default:
        assert(!"unreachable");
        break;
    }
  }

  if (found_arg_headless && found_arg_display_mode)
    printf("options -n and -m are mutually exclusive\n");

  if (optind != argc)
    printf("trailing args\n");
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
  return display->init(this, renderer);
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

void Application::mainloop() {
  display->loop(this, renderer);
}
}

