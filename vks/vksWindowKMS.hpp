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

#include <vulkan/vulkan_intel.h>

#include "vksWindow.hpp"

#include "vksApplication.hpp"

static void
page_flip_handler(int fd, unsigned int frame,
                  unsigned int sec, unsigned int usec, void *data)
{}

static struct termios save_tio;

namespace vks {
class WindowKMS : public Window {

  drmModeCrtc *crtc;
  drmModeConnector *connector;

  struct gbm_device *gbm_dev;
  struct gbm_bo *gbm_bo;

  int fd;

  struct kms_buffer {
    struct gbm_bo *gbm_bo;
    VkDeviceMemory mem;
    uint32_t fb;
    uint32_t stride;
  };

  struct render_buffer {
     VkImage image;
     VkImageView view;
     //VkFramebuffer framebuffer;
  };

  #define MAX_NUM_IMAGES 3

  struct kms_buffer kms_buffers[MAX_NUM_IMAGES];
  struct render_buffer render_buffers[MAX_NUM_IMAGES];

public:
  WindowKMS() {
    gbm_dev = NULL;
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

  void init_swap_chain(Renderer *r) {

  }

  virtual void update_window_title(const std::string& title) {
  }

  virtual void iterate(Application *app) {

  }

  void loop(vks::Application *app)
  {
    int len, ret;
    char buf[16];
    struct pollfd pfd[2];
    struct CubeBuffer *b;
    struct kms_buffer *kms_b;

    fprintf(stderr, "starting renderLoop\n");

    pfd[0].fd = STDIN_FILENO;
    pfd[0].events = POLLIN;
    pfd[1].fd = fd;
    pfd[1].events = POLLIN;

    drmEventContext evctx = {};
    evctx.version = 2;
    evctx.page_flip_handler = page_flip_handler;

    ret = drmModeSetCrtc(fd, crtc->crtc_id, kms_buffers[0].fb,
        0, 0, &connector->connector_id, 1, &crtc->mode);
    vik_log_f_if(ret < 0, "modeset failed: %m");


    ret = drmModePageFlip(fd, crtc->crtc_id, kms_buffers[0].fb,
        DRM_MODE_PAGE_FLIP_EVENT, NULL);
    vik_log_f_if(ret < 0, "pageflip failed: %m\n");

    vik_log_d("renderLoop: init done");

    while (1) {
      ret = poll(pfd, 2, -1);
      vik_log_f_if(ret == -1, "poll failed");
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
        //b = &vc->buffers[app->frameCounter & 1];
        kms_b = &kms_buffers[app->renderer->timer.frames_since_tick & 1];

        //app->model.render(vc, b);

        vik_log_d("renderLoop: render");
        app->render();

        vik_log_d("renderLoop: drmModePageFlip");
        ret = drmModePageFlip(fd, crtc->crtc_id, kms_b->fb,
                              DRM_MODE_PAGE_FLIP_EVENT, NULL);
        vik_log_f_if(ret < 0, "pageflip failed: %m");
        app->renderer->timer.increment();
      }
    }

    vik_log_d("renderLoop done");

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
  int init(vks::Application *app) {

    vik_log_d("init");

    drmModeRes *resources;
    drmModeEncoder *encoder;
    int i;

    if (init_vt() == -1)
      return -1;

    vik_log_d("init vt done");

    fd = open("/dev/dri/card0", O_RDWR);
    vik_log_f_if(fd == -1, "failed to open /dev/dri/card0");

    /* Get KMS resources and find the first active connecter. We'll use that
      connector and the crtc driving it in the mode it's currently running. */
    resources = drmModeGetResources(fd);
    //resources = nullptr;
    vik_log_f_if(!resources, "drmModeGetResources failed: %s", strerror(errno));

    for (i = 0; i < resources->count_connectors; i++) {
      connector = drmModeGetConnector(fd, resources->connectors[i]);
      if (connector->connection == DRM_MODE_CONNECTED)
        break;
      drmModeFreeConnector(connector);
      connector = NULL;
    }

    vik_log_f_if(!connector, "no connected connector!");
    encoder = drmModeGetEncoder(fd, connector->encoder_id);
    vik_log_f_if(!encoder, "failed to get encoder");
    crtc = drmModeGetCrtc(fd, encoder->crtc_id);
    vik_log_f_if(!crtc, "failed to get crtc");
    vik_log_i("mode info: hdisplay %d, vdisplay %d",
           crtc->mode.hdisplay, crtc->mode.vdisplay);

    app->renderer->width = crtc->mode.hdisplay;
    app->renderer->height = crtc->mode.vdisplay;

    gbm_dev = gbm_create_device(fd);

    /*
    vc->init_vk(NULL);
    vc->image_format = VK_FORMAT_R8G8B8A8_SRGB;
    vc->init_vk_objects(&app->model);
*/

    PFN_vkCreateDmaBufImageINTEL create_dma_buf_image =
        (PFN_vkCreateDmaBufImageINTEL)vkGetDeviceProcAddr(app->renderer->device, "vkCreateDmaBufImageINTEL");

    /*
    error("Swap chain images: %d\n", app->swapChain.imageCount);
    for (VkImage img : app->swapChain.images) {
      error("VkImage %p\n", img);
    }
    */

    //VkImage dmaBufImages[2];

    app->renderer->frameBuffers.resize(1);


    for (uint32_t i = 0; i < 2; i++) {
      //struct CubeBuffer *b = &vc->buffers[i];
      struct kms_buffer *kms_b = &kms_buffers[i];
      struct render_buffer *b = &render_buffers[i];
      int buffer_fd, stride, ret;

      kms_b->gbm_bo = gbm_bo_create(gbm_dev, app->renderer->width, app->renderer->height,
                                    GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT);

      vik_log_d("Created kms buffer %p", kms_b);

      buffer_fd = gbm_bo_get_fd(kms_b->gbm_bo);
      stride = gbm_bo_get_stride(kms_b->gbm_bo);


      VkDmaBufImageCreateInfo dmaBufInfo = {};

      VkExtent3D extent = {};
      extent.width = app->renderer->width;
      extent.height = app->renderer->height;
      extent.depth = 1;

      //error("kms: Using color format %d\n", app->swapChain.colorFormat);

      dmaBufInfo.sType = (VkStructureType) VK_STRUCTURE_TYPE_DMA_BUF_IMAGE_CREATE_INFO_INTEL;
      dmaBufInfo.fd = buffer_fd;
      //dmaBufInfo.format = app->swapChain.colorFormat;
      dmaBufInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
      dmaBufInfo.extent = extent;
      dmaBufInfo.strideInBytes = stride;

      vik_log_d("Creating dmabuf image %d", i);
      create_dma_buf_image(app->renderer->device,
                           &dmaBufInfo,
                           NULL,
                           &kms_b->mem,
                           &b->image
                           //&app->swapChain.images.at(i)
                           //&dmaBufImages[i]
                           );

      vik_log_d("Created image %p", &b->image);

      close(buffer_fd);

      kms_b->stride = gbm_bo_get_stride(kms_b->gbm_bo);
      uint32_t bo_handles[4] = { (uint32_t) (gbm_bo_get_handle(kms_b->gbm_bo).s32), };
      uint32_t pitches[4] = { (uint32_t) stride, };
      uint32_t offsets[4] = { 0, };
      ret = drmModeAddFB2(fd, app->renderer->width, app->renderer->height,
                          DRM_FORMAT_XRGB8888, bo_handles,
                          pitches, offsets, &kms_b->fb, 0);
      vik_log_f_if(ret == -1, "drmModeAddFB2 failed");

      //vc->init_buffer(b);
      init_buffer(app, &render_buffers[i]);
    }

    vik_log_d("setupWindow successfull");

    return 0;
  }

  void init_buffer(vks::Application *app, struct render_buffer *b) {

    VkComponentMapping component_mapping;
    component_mapping.r = VK_COMPONENT_SWIZZLE_R;
    component_mapping.g = VK_COMPONENT_SWIZZLE_G;
    component_mapping.b = VK_COMPONENT_SWIZZLE_B;
    component_mapping.a = VK_COMPONENT_SWIZZLE_A;

    VkImageSubresourceRange subresource_range;
    subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresource_range.baseMipLevel = 0;
    subresource_range.levelCount = 1;
    subresource_range.baseArrayLayer = 0;
    subresource_range.layerCount = 1;

    VkImageViewCreateInfo imageviewinfo;
    imageviewinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageviewinfo.image = b->image;
    imageviewinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageviewinfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageviewinfo.components = component_mapping;
    imageviewinfo.subresourceRange = subresource_range;

    vkCreateImageView(app->renderer->device,
                      &imageviewinfo,
                      NULL,
                      &b->view);

    VkFramebufferCreateInfo framebufferinfo;
    framebufferinfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferinfo.renderPass = app->renderer->renderPass;
    framebufferinfo.attachmentCount = 1;
    framebufferinfo.pAttachments = &b->view;
    framebufferinfo.width = app->renderer->width;
    framebufferinfo.height = app->renderer->height;
    framebufferinfo.layers = 1;

    vkCreateFramebuffer(app->renderer->device,
                        &framebufferinfo,
                        NULL,
                        &app->renderer->frameBuffers[0]);

    vik_log_d("init framebuffer %p done.", &app->renderer->frameBuffers[0]);
  }

  const std::vector<const char*> required_extensions() {
    return std::vector<const char*>();
  }

};
}
