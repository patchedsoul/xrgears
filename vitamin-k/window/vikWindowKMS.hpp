/*
 * vitamin-k
 *
 * Copyright (C) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (C) 2012 Rob Clark <rob@ti.com>
 * Copyright (C) 2015 Intel Corporation
 * Copyright (C) 2017 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This code is licensed under the GNU General Public License Version 3 (GPLv3)
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Based on kmscube example written by Rob Clark, based on test app originally
 * written by Arvin Schnell.
 * Based on vkcube example.
 */

#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <poll.h>
#include <signal.h>

#include <gbm.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/major.h>

#include <string>
#include <vector>

#include "vikWindow.hpp"

#include "../system/vikLog.hpp"
#include "../render/vikSwapChainDRM.hpp"

static void
page_flip_handler(int fd, unsigned int frame,
                  unsigned int sec, unsigned int usec, void *data)
{}

static struct termios save_tio;

namespace vik {
class WindowKMS : public Window {
  drmModeCrtc *crtc;
  drmModeConnector *connector;

  gbm_device *gbm_dev;
  gbm_bo *gbm_buffer;

  int fd;

  pollfd pfd[2];
  drmEventContext evctx;

  SwapChainDRM swap_chain;

 public:
  explicit WindowKMS(Settings *s) : Window(s) {
    gbm_dev = NULL;
    name = "kms";

    evctx = {};
    evctx.version = 2;
    evctx.page_flip_handler = page_flip_handler;

    pfd[0].fd = STDIN_FILENO;
    pfd[0].events = POLLIN;
    pfd[1].events = POLLIN;
  }

  ~WindowKMS() {}

  static void restore_vt(void) {
    struct vt_mode mode = { .mode = VT_AUTO };
    ioctl(STDIN_FILENO, VT_SETMODE, &mode);

    tcsetattr(STDIN_FILENO, TCSANOW, &save_tio);
    ioctl(STDIN_FILENO, KDSETMODE, KD_TEXT);
  }

  static void handle_signal(int sig) {
    restore_vt();
  }

  int init_vt() {
    struct termios tio;
    struct stat buf;
    int ret;

    /* First, save term io setting so we can restore properly. */
    tcgetattr(STDIN_FILENO, &save_tio);

    /* Make sure we're on a vt. */
    ret = fstat(STDIN_FILENO, &buf);
    vik_log_f_if(ret == -1, "failed to stat stdin");

    if (major(buf.st_rdev) != TTY_MAJOR) {
      vik_log_e("stdin not a vt, running in no-display mode");
      return -1;
    }

    atexit(restore_vt);

    /* Set console input to raw mode. */
    tio = save_tio;
    tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &tio);

    /* Restore console on SIGINT and friends. */
    struct sigaction act = {};
    act.sa_handler = handle_signal;
    act.sa_flags = (int) SA_RESETHAND;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGABRT, &act, NULL);

    /* We don't drop drm master, so block VT switching while we're
   * running. Otherwise, switching to X on another VT will crash X when it
   * fails to get drm master. */
    struct vt_mode mode = {};
    mode.mode = VT_PROCESS;
    mode.relsig = 0;
    mode.acqsig = 0;
    ret = ioctl(STDIN_FILENO, VT_SETMODE, &mode);
    vik_log_f_if(ret == -1, "failed to take control of vt handling\n");

    /* Set KD_GRAPHICS to disable fbcon while we render. */
    ret = ioctl(STDIN_FILENO, KDSETMODE, KD_GRAPHICS);
    vik_log_f_if(ret == -1, "failed to switch console to graphics mode\n");

    return 0;
  }

  // Return -1 on failure.
  int init(uint32_t width, uint32_t height) {
    drmModeRes *resources;
    drmModeEncoder *encoder;
    int i;

    if (init_vt() == -1)
      return -1;

    fd = open("/dev/dri/card0", O_RDWR);
    vik_log_f_if(fd == -1, "failed to open /dev/dri/card0\n");

    pfd[1].fd = fd;

    /* Get KMS resources and find the first active connecter. We'll use that
      connector and the crtc driving it in the mode it's currently running. */
    resources = drmModeGetResources(fd);
    vik_log_f_if(!resources, "drmModeGetResources failed: %s", strerror(errno));

    for (i = 0; i < resources->count_connectors; i++) {
      connector = drmModeGetConnector(fd, resources->connectors[i]);
      if (connector->connection == DRM_MODE_CONNECTED)
        break;
      drmModeFreeConnector(connector);
      connector = NULL;
    }

    vik_log_f_if(!connector, "no connected connector!\n");
    encoder = drmModeGetEncoder(fd, connector->encoder_id);
    vik_log_f_if(!encoder, "failed to get encoder\n");
    crtc = drmModeGetCrtc(fd, encoder->crtc_id);
    vik_log_f_if(!crtc, "failed to get crtc\n");
    vik_log_i("mode info: hdisplay %d, vdisplay %d",
              crtc->mode.hdisplay, crtc->mode.vdisplay);

    size_only_cb(crtc->mode.hdisplay, crtc->mode.vdisplay);

    gbm_dev = gbm_create_device(fd);

    return 0;
  }

  const std::vector<const char*> required_extensions() {
    return {};
  }

  void init_swap_chain(uint32_t width, uint32_t height) {
    swap_chain.surface_format.format = VK_FORMAT_B8G8R8A8_SRGB;
    swap_chain.init(swap_chain.device, swap_chain.surface_format.format, gbm_dev, fd,
             width, height);
    swap_chain.set_mode_and_page_flip(fd, crtc, connector);
  }

  SwapChain* get_swap_chain() {
    return (SwapChain*) &swap_chain;
  }

  void update_window_title(const std::string& title) {}

  void poll_events() {
    char buf[16];
    int len = read(STDIN_FILENO, buf, sizeof(buf));
    vik_log_d("== PRESSING |%c|", buf[0]);
    switch (buf[0]) {
      case 'q':
        quit_cb();
        break;
      case 'w':
        keyboard_key_cb(Input::W, true);
        break;
      case '\e':
        if (len == 1)
          quit_cb();
    }
  }

  void render() {
    drmHandleEvent(fd, &evctx);
    swap_chain.render(fd, crtc->crtc_id);
  }

  void iterate() {
    int ret = poll(pfd, 2, -1);
    vik_log_f_if(ret == -1, "poll failed");
    if (pfd[0].revents & POLLIN)
      poll_events();
    if (pfd[1].revents & POLLIN)
      render();
  }

  VkBool32 check_support(VkPhysicalDevice physical_device) {
    return true;
  }
};
}
