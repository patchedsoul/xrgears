#include <assert.h>

#include "vkcApplication.hpp"

#include "vkcCube.hpp"
#include "vkcWindowXCB.hpp"
#include "vkcWindowKMS.hpp"
#include "vkcWindowWayland.hpp"
#include "vkcRenderer.hpp"

CubeApplication::CubeApplication(uint32_t w, uint32_t h) {
  mode = DISPLAY_MODE_AUTO;
  renderer = new CubeRenderer(w, h);
}

CubeApplication::~CubeApplication() {
  delete renderer;
}

bool CubeApplication::display_mode_from_string(const char *s) {
  if (streq(s, "auto")) {
    mode = DISPLAY_MODE_AUTO;
    return true;
  } else if (streq(s, "kms")) {
    mode = DISPLAY_MODE_KMS;
    return true;
  } else if (streq(s, "xcb")) {
    mode = DISPLAY_MODE_XCB;
    return true;
  } else if (streq(s, "wayland")) {
    mode = DISPLAY_MODE_WAYLAND;
    return true;
  } else {
    return false;
  }
}

void CubeApplication::parse_args(int argc, char *argv[]) {
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


int CubeApplication::init_display_mode(display_mode_type m) {
  switch (mode) {
    case DISPLAY_MODE_KMS:
      display = new VikDisplayModeKMS();
      break;
    case DISPLAY_MODE_XCB:
      display = new VikDisplayModeXCB();
      break;
    case DISPLAY_MODE_WAYLAND:
      display = new VikDisplayModeWayland();
      break;
    case DISPLAY_MODE_AUTO:
      return -1;
  }
  return display->init(this, renderer);
}

void CubeApplication::init_display_mode_auto() {
  mode = DISPLAY_MODE_WAYLAND;
  if (init_display_mode(mode) == -1) {
    vik_log_e("failed to initialize wayland, falling back to xcb");
    delete(display);
    mode = DISPLAY_MODE_XCB;
    if (init_display_mode(mode) == -1) {
      vik_log_e("failed to initialize xcb, falling back to kms");
      delete(display);
      mode = DISPLAY_MODE_KMS;
      init_display_mode(mode);
    }
  }
}

void CubeApplication::init_display() {
  if (mode == DISPLAY_MODE_AUTO)
    init_display_mode_auto();
  else if (init_display_mode(mode) == -1)
    vik_log_f("failed to initialize %s", display->name.c_str());
}

void CubeApplication::mainloop() {
  display->loop(this, renderer);
}


