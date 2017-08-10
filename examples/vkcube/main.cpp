/*
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (c) 2012 Rob Clark <rob@ti.com>
 * Copyright © 2015 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Based on kmscube example written by Rob Clark, based on test app originally
 * written by Arvin Schnell.
 *
 * Compile and run this with minigbm:
 *
 *   https://chromium.googlesource.com/chromiumos/platform/minigbm
 *
 * Edit the minigbm Makefile to add -DGBM_I915 to CPPFLAGS, then compile and
 * install with make DESTDIR=<some path>. Then pass --with-minigbm=<some path>
 * to configure when configuring vkcube
 */

#include <assert.h>

#include "xcb.hpp"
#include "kms.hpp"

static display_mode_type my_display_mode = DISPLAY_MODE_AUTO;

VikDisplayMode *display;

static inline bool
streq(const char *a, const char *b)
{
  return strcmp(a, b) == 0;
}

static bool
display_mode_from_string(const char *s, enum display_mode_type *mode)
{
  if (streq(s, "auto")) {
    *mode = DISPLAY_MODE_AUTO;
    return true;
  } else if (streq(s, "kms")) {
    *mode = DISPLAY_MODE_KMS;
    return true;
  } else if (streq(s, "xcb")) {
    *mode = DISPLAY_MODE_XCB;
    return true;
  } else {
    return false;
  }
}

void
parse_args(int argc, char *argv[])
{
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
        if (!display_mode_from_string(optarg, &my_display_mode))
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


void
init_display(CubeApplication *vc, VikRenderer* renderer, enum display_mode_type *mode)
{
  switch (*mode) {
    case DISPLAY_MODE_AUTO:

      fprintf(stderr, "failed to initialize wayland, falling back "
                      "to xcb\n");
      *mode = DISPLAY_MODE_XCB;

      display = new VikDisplayModeXCB();
      if (display->init(vc, renderer) == -1) {
        fprintf(stderr, "failed to initialize xcb, falling back "
                        "to kms\n");
        delete(display);
        *mode = DISPLAY_MODE_KMS;
        display = new VikDisplayModeKMS();
        if (display->init(vc, renderer) == -1) {
          fprintf(stderr, "failed to initialize kms\n");
        }
      }
      break;
    case DISPLAY_MODE_KMS:
      display = new VikDisplayModeKMS();
      if (display->init(vc, renderer) == -1)
        fail("failed to initialize kms");
      break;
    case DISPLAY_MODE_XCB:
      display = new VikDisplayModeXCB();
      if (display->init(vc, renderer) == -1)
        printf("failed to initialize xcb\n");
        fail("failed to initialize xcb");
      break;
  }
}

void
mainloop(CubeApplication *vc, enum display_mode_type mode)
{
  switch (mode) {
    case DISPLAY_MODE_AUTO:
      assert(!"display mode is unset");
      break;
    case DISPLAY_MODE_XCB:
    case DISPLAY_MODE_KMS:
      display->main_loop(vc, vc->renderer);
      break;
  }
}

//

int main(int argc, char *argv[])
{
  CubeApplication vc(1280, 720);

  parse_args(argc, argv);

  init_display(&vc, vc.renderer, &my_display_mode);
  printf("Starting main loop\n");
  mainloop(&vc, my_display_mode);

  return 0;
}
