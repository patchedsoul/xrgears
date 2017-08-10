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

#include <getopt.h>
#include <assert.h>
#include <stdio.h>

#include "vk.hpp"
#include "xcb.hpp"
#include "kms.hpp"

extern struct model cube_model;

static display_mode my_display_mode = DISPLAY_MODE_AUTO;

VikDisplayModeXCB *display_xcb;
VikDisplayModeKMS *display_kms;

static inline bool
streq(const char *a, const char *b)
{
  return strcmp(a, b) == 0;
}

static bool
display_mode_from_string(const char *s, enum display_mode *mode)
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


//
void
init_display(struct vkcube *vc, enum display_mode *mode)
{
  switch (*mode) {
    case DISPLAY_MODE_AUTO:

      fprintf(stderr, "failed to initialize wayland, falling back "
                      "to xcb\n");
      *mode = DISPLAY_MODE_XCB;
      display_xcb = new VikDisplayModeXCB();
      if (display_xcb->init(vc) == -1) {
        fprintf(stderr, "failed to initialize xcb, falling back "
                        "to kms\n");
        *mode = DISPLAY_MODE_KMS;
        display_kms = new VikDisplayModeKMS();
        if (display_kms->init(vc) == -1) {
          fprintf(stderr, "failed to initialize kms\n");
        }
      }
      break;
    case DISPLAY_MODE_KMS:
      display_kms = new VikDisplayModeKMS();
      if (display_kms->init(vc) == -1)
        fail("failed to initialize kms");
      break;
    case DISPLAY_MODE_XCB:
      display_xcb = new VikDisplayModeXCB();
      if (display_xcb->init(vc) == -1)
        fail("failed to initialize xcb");
      break;
  }
}

void
mainloop(struct vkcube *vc, enum display_mode mode)
{
  switch (mode) {
    case DISPLAY_MODE_AUTO:
      assert(!"display mode is unset");
      break;
    case DISPLAY_MODE_XCB:
      display_xcb->main_loop(vc);
      break;
    case DISPLAY_MODE_KMS:
     display_kms->main_loop(vc);
      break;
  }
}

//

int main(int argc, char *argv[])
{
  struct vkcube vc;

  parse_args(argc, argv);

  vc.model = cube_model;
  vc.width = 1024;
  vc.height = 768;
  gettimeofday(&vc.start_tv, NULL);

  init_display(&vc, &my_display_mode);
  mainloop(&vc, my_display_mode);

  return 0;
}
