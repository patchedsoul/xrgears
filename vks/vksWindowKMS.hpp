/*
 * XRGears
 *
 * Copyright (C) 2016 Sascha Willems - www.saschawillems.de
 * Copyright (C) 2017 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 *
 * Based on Vulkan Examples written by Sascha Willems
 */

#pragma once

//#include <vulkan/vulkan.h>
#include <sys/sysmacros.h>

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/major.h>
#include <termios.h>
#include <poll.h>
#include <math.h>
#include <assert.h>
#include <sys/mman.h>
#include <linux/input.h>

#include <stdarg.h>
#include <stdnoreturn.h>
#include <sys/time.h>
#include <stdbool.h>
#include <string.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <png.h>

#include <drm_fourcc.h>

#include <xcb/xcb.h>

#include <wayland-client.h>

//#define VK_USE_PLATFORM_XCB_KHR
//#define VK_USE_PLATFORM_WAYLAND_KHR
#define VK_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_intel.h>

#include <gbm.h>

#include "vksApplication.hpp"

static struct termios save_tio;

class ApplicationKMS  : public Application {
  int fd;
  drmModeCrtc *crtc;
  drmModeConnector *connector;
  VkFormat image_format;
  int current;
  VkRenderPass render_pass;

  struct gbm_device *gbm_device;

  struct vkcube_buffer {
    struct gbm_bo *gbm_bo;
    VkDeviceMemory mem;
    VkImage image;
    VkImageView view;
    VkFramebuffer framebuffer;
    uint32_t fb;
    uint32_t stride;
  };

#define MAX_NUM_IMAGES 4
  struct vkcube_buffer buffers[MAX_NUM_IMAGES];

public:
  explicit ApplicationKMS(bool enableValidation) : Application(enableValidation) {

    init_kms();

  }
  ~ApplicationKMS() {}

  const char* requiredExtensionName() {
    return VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
  }

  void initSwapChain() {
    //createDirect2DisplaySurface(width, height);
    //swapChain.initSurfaceCommon();
  }

  void renderLoop() {
    int len, ret;
    char buf[16];
    struct pollfd pfd[2];
    struct vkcube_buffer *b;

    pfd[0].fd = STDIN_FILENO;
    pfd[0].events = POLLIN;
    pfd[1].fd = fd;
    pfd[1].events = POLLIN;

    drmEventContext evctx = {
      .version = 2,
      .vblank_handler = NULL,
      .page_flip_handler = page_flip_handler,
      .page_flip_handler2 = NULL
    };

    ret = drmModeSetCrtc(fd, crtc->crtc_id, buffers[0].fb,
        0, 0, &connector->connector_id, 1, &crtc->mode);
    fail_if(ret < 0, "modeset failed: %m\n");


    ret = drmModePageFlip(fd, crtc->crtc_id, buffers[0].fb,
        DRM_MODE_PAGE_FLIP_EVENT, NULL);
    fail_if(ret < 0, "pageflip failed: %m\n");

    while (1) {
      ret = poll(pfd, 2, -1);
      fail_if(ret == -1, "poll failed\n");
      if (pfd[0].revents & POLLIN) {
        len = read(STDIN_FILENO, buf, sizeof(buf));
        switch (buf[0]) {
          case 'q':
            return;
          case '\e':
            if (len == 1)
              return;
        }
      }
      if (pfd[1].revents & POLLIN) {
        drmHandleEvent(fd, &evctx);
        b = &buffers[current & 1];

        //model.render(vc, b);
        render();

        ret = drmModePageFlip(fd, crtc->crtc_id, b->fb,
                              DRM_MODE_PAGE_FLIP_EVENT, NULL);
        fail_if(ret < 0, "pageflip failed: %m\n");
        current++;
      }
    }
  }
/*
  void renderLoop() {
    while (!quit) {
      auto tStart = std::chrono::high_resolution_clock::now();
      if (viewUpdated) {
        viewUpdated = false;
        viewChanged();
      }
      render();
      frameCounter++;
      auto tEnd = std::chrono::high_resolution_clock::now();
      auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
      frameTimer = tDiff / 1000.0f;
      camera.update(frameTimer);
      if (camera.moving())
        viewUpdated = true;
      // Convert to clamped timer value
      if (!paused) {
        timer += timerSpeed * frameTimer;
        if (timer > 1.0)
          timer -= 1.0f;
      }
      fpsTimer += (float)tDiff;
      if (fpsTimer > 1000.0f) {
        lastFPS = frameCounter;
        updateTextOverlay();
        fpsTimer = 0.0f;
        frameCounter = 0;
      }
    }
  }
*/
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
    if (ret == -1)
      printf("failed to stat stdin\n");

    if (major(buf.st_rdev) != TTY_MAJOR) {
      fprintf(stderr, "stdin not a vt, running in no-display mode\n");
      return -1;
    }

    atexit(restore_vt);

    /* Set console input to raw mode. */
    tio = save_tio;
    tio.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &tio);

    /* Restore console on SIGINT and friends. */

    /*
    struct sigaction act = {
      .sa_handler = handle_signal,
          .sa_flags = SA_RESETHAND
    };
    */

    struct sigaction act = {};
    act.sa_handler = handle_signal;
    act.sa_flags = SA_RESETHAND;

    sigaction(SIGINT, &act, NULL);
    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGABRT, &act, NULL);

    /* We don't drop drm master, so block VT switching while we're
      * running. Otherwise, switching to X on another VT will crash X when it
      * fails to get drm master. */

    /*
    struct vt_mode mode = {
          .mode = VT_PROCESS,
          .relsig = 0,
          .acqsig = 0
    };
    */

     vt_mode mode = {};
     mode.mode = VT_PROCESS;
     mode.relsig = 0;
     mode.acqsig = 0;

     /*
    struct vt_mode mode = {
          .mode = VT_PROCESS,
          .relsig = 0,
          .acqsig = 0
    };*/

    ret = ioctl(STDIN_FILENO, VT_SETMODE, &mode);

    if (ret == -1)
      printf("failed to take control of vt handling\n");

    /* Set KD_GRAPHICS to disable fbcon while we render. */
    ret = ioctl(STDIN_FILENO, KDSETMODE, KD_GRAPHICS);

    if (ret == -1)
      printf("failed to switch console to graphics mode\n");

    return 0;
  }

  static void
  page_flip_handler(int fd, unsigned int frame,
                    unsigned int sec, unsigned int usec, void *data)
  {
  }

  void fail_if(bool condition, const char* msg, ...) {
    if (condition) {
      printf(msg);
      exit(0);
    }
  }

  int init_kms() {
    drmModeRes *resources;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    int i;

    if (init_vt() == -1)
      return -1;

    fd = open("/dev/dri/card0", O_RDWR);
    fail_if(fd == -1, "failed to open /dev/dri/card0\n");

    /* Get KMS resources and find the first active connecter. We'll use that
  connector and the crtc driving it in the mode it's currently running. */
    resources = drmModeGetResources(fd);
    fail_if(!resources, "drmModeGetResources failed: %s\n", strerror(errno));

    printf("We have %d connectors.\n", resources->count_connectors);

    for (i = 0; i < resources->count_connectors; i++) {
      connector = drmModeGetConnector(fd, resources->connectors[i]);
      if (connector->connection == DRM_MODE_CONNECTED)
        break;
      drmModeFreeConnector(connector);
      connector = NULL;
    }

    fail_if(!connector, "no connected connector!\n");
    encoder = drmModeGetEncoder(fd, connector->encoder_id);
    fail_if(!encoder, "failed to get encoder\n");
    crtc = drmModeGetCrtc(fd, encoder->crtc_id);
    fail_if(!crtc, "failed to get crtc\n");
    printf("mode info: hdisplay %d, vdisplay %d\n",
           crtc->mode.hdisplay, crtc->mode.vdisplay);

    this->connector = connector;
    width = crtc->mode.hdisplay;
    height = crtc->mode.vdisplay;

    gbm_device = gbm_create_device(fd);

    //init_vk(NULL);
    image_format = VK_FORMAT_R8G8B8A8_SRGB;
    //init_vk_objects(vc);

    PFN_vkCreateDmaBufImageINTEL create_dma_buf_image =
        (PFN_vkCreateDmaBufImageINTEL)vkGetDeviceProcAddr(device, "vkCreateDmaBufImageINTEL");

    for (uint32_t i = 0; i < 2; i++) {
      struct vkcube_buffer *b = &buffers[i];
      int fd, stride, ret;

      b->gbm_bo = gbm_bo_create(gbm_device, width, height,
                                GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT);

      fd = gbm_bo_get_fd(b->gbm_bo);
      stride = gbm_bo_get_stride(b->gbm_bo);

      VkDmaBufImageCreateInfo imgCreateInfo = {};
      imgCreateInfo.sType = (VkStructureType) VK_STRUCTURE_TYPE_DMA_BUF_IMAGE_CREATE_INFO_INTEL;
      imgCreateInfo.fd = fd;
      imgCreateInfo.format = image_format;
      imgCreateInfo.extent = { width, height, 1 };
      imgCreateInfo.strideInBytes = stride;

      create_dma_buf_image(device,
                           &imgCreateInfo,
                           NULL,
                           &b->mem,
                           &b->image);
      close(fd);

      b->stride = gbm_bo_get_stride(b->gbm_bo);
      uint32_t bo_handles[4] = { (uint32_t) gbm_bo_get_handle(b->gbm_bo).s32, };
      uint32_t pitches[4] = { (uint32_t) stride, };
      uint32_t offsets[4] = { 0, };
      ret = drmModeAddFB2(fd, width, height,
                          DRM_FORMAT_XRGB8888, bo_handles,
                          pitches, offsets, &b->fb, 0);
      fail_if(ret == -1, "addfb2 failed\n");

      init_buffer(b);
    }

    return 0;
  }

  void init_buffer(struct vkcube_buffer *b) {
    struct VkImageViewCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .image = b->image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = image_format,
      .components = {
        .r = VK_COMPONENT_SWIZZLE_R,
        .g = VK_COMPONENT_SWIZZLE_G,
        .b = VK_COMPONENT_SWIZZLE_B,
        .a = VK_COMPONENT_SWIZZLE_A,
      },
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      }
    };
    vkCreateImageView(device, &info, NULL, &b->view);

     struct VkFramebufferCreateInfo fbInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderPass = render_pass,
        .attachmentCount = 1,
        .pAttachments = &b->view,
        .width = width,
        .height = height,
        .layers = 1
     };
     vkCreateFramebuffer(device, &fbInfo, NULL, &b->framebuffer);
  }

};
