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

#include "vkcWindow.hpp"
#include "vkcApplication.hpp"
#include "vkcRenderer.hpp"
#include "vksLog.hpp"

static void
page_flip_handler(int fd, unsigned int frame,
                  unsigned int sec, unsigned int usec, void *data)
{}

static struct termios save_tio;

namespace vkc {
class WindowKMS : public Window {

  drmModeCrtc *crtc;
  drmModeConnector *connector;

  gbm_device *gbm_dev;
  gbm_bo *gbm_buffer;

  bool quit = false;

  int fd;

  pollfd pfd[2];
  drmEventContext evctx;

public:
  WindowKMS() {
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
  int init(Renderer* r) {
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

    r->width = crtc->mode.hdisplay;
    r->height = crtc->mode.vdisplay;

    gbm_dev = gbm_create_device(fd);

    r->init_vk(NULL);
    r->image_format = VK_FORMAT_R8G8B8A8_SRGB;

    r->swap_chain = new SwapChainDRM();

    init_cb();

    SwapChainDRM *sc = (SwapChainDRM*) r->swap_chain;
    sc->init(r->device, r->image_format, gbm_dev, fd,
             r->width, r->height, r->render_pass);
    sc->set_mode_and_page_flip(fd, crtc, connector);

    return 0;
  }

  void poll_events() {
    char buf[16];
    int len = read(STDIN_FILENO, buf, sizeof(buf));
    vik_log_d("== PRESSING |%c|", buf[0]);
    switch (buf[0]) {
      case 'q':
        quit = 1;
      case '\e':
        if (len == 1)
          quit = 1;
    }
  }

  void render(Renderer *r) {
    drmHandleEvent(fd, &evctx);

    update_cb();

    SwapChainDRM *sc = (SwapChainDRM*) r->swap_chain;

    int index = sc->current & 1;

    r->render(index);
    kms_buffer *kms_b = &sc->kms_buffers[index];
    int ret = drmModePageFlip(fd, crtc->crtc_id, kms_b->fb,
                              DRM_MODE_PAGE_FLIP_EVENT, NULL);
    vik_log_f_if(ret < 0, "pageflip failed: %m");
    sc->current++;
  }

  void poll_and_render(Renderer *r) {
    int ret = poll(pfd, 2, -1);
    vik_log_f_if(ret == -1, "poll failed");
    if (pfd[0].revents & POLLIN)
      poll_events();
    if (pfd[1].revents & POLLIN)
      render(r);
  }

  void loop(Renderer *r) {
    while (1) {
      poll_and_render(r);
      if (quit)
        return;
    }
  }

};
}
