#include <assert.h>

#include "application.hpp"

#include "cube.hpp"
#include "xcb.hpp"
#include "kms.hpp"
#include "VikRenderer.hpp"

CubeApplication::CubeApplication(uint32_t w, uint32_t h) {
  mode = DISPLAY_MODE_AUTO;
  renderer = new VikRenderer(w, h);
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


void CubeApplication::init_display() {
  switch (mode) {
    case DISPLAY_MODE_AUTO:

      fprintf(stderr, "failed to initialize wayland, falling back "
                      "to xcb\n");
      mode = DISPLAY_MODE_XCB;

      display = new VikDisplayModeXCB();
      if (display->init(this, renderer) == -1) {
        fprintf(stderr, "failed to initialize xcb, falling back "
                        "to kms\n");
        delete(display);
        mode = DISPLAY_MODE_KMS;
        display = new VikDisplayModeKMS();
        if (display->init(this, renderer) == -1) {
          fprintf(stderr, "failed to initialize kms\n");
        }
      }
      break;
    case DISPLAY_MODE_KMS:
      display = new VikDisplayModeKMS();
      if (display->init(this, renderer) == -1)
        fail("failed to initialize kms");
      break;
    case DISPLAY_MODE_XCB:
      display = new VikDisplayModeXCB();
      if (display->init(this, renderer) == -1)
        printf("failed to initialize xcb\n");
      fail("failed to initialize xcb");
      break;
  }
}

void CubeApplication::mainloop() {
  switch (mode) {
    case DISPLAY_MODE_AUTO:
      assert(!"display mode is unset");
      break;
    case DISPLAY_MODE_XCB:
    case DISPLAY_MODE_KMS:
      display->main_loop(this, renderer);
      break;
  }
}


