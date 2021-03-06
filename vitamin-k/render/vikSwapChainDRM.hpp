/*
 * vitamin-k
 *
 * Copyright 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright 2012 Rob Clark <rob@ti.com>
 * Copyright 2015 Intel Corporation
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 *
 * Based on vkcube example.
 */

#pragma once

#include <gbm.h>
#include <vulkan/vulkan_intel.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <poll.h>
#include <signal.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_fourcc.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/major.h>

#include <vector>

#include "vikSwapChain.hpp"

namespace vik {

struct KMSBuffer {
  gbm_bo *gbm_buffer;
  VkDeviceMemory mem;
  uint32_t fb;
  uint32_t stride;
};

class SwapChainDRM : public SwapChain {
 public:
  std::vector<KMSBuffer> kms_buffers;
  int current;

  SwapChainDRM() {
    image_count = 4;
    buffers.resize(image_count);
    kms_buffers.resize(image_count);
  }

  ~SwapChainDRM() {}

  void cleanup() {}

  void create(uint32_t width, uint32_t height) {}

  void init(VkDevice device, VkFormat image_format, gbm_device *gbm_dev, int fd,
            uint32_t width, uint32_t height) {
    PFN_vkCreateDmaBufImageINTEL create_dma_buf_image =
        (PFN_vkCreateDmaBufImageINTEL)vkGetDeviceProcAddr(device, "vkCreateDmaBufImageINTEL");

    for (uint32_t i = 0; i < image_count; i++) {
      SwapChainBuffer *b = &buffers[i];
      KMSBuffer *kms_b = &kms_buffers[i];
      int buffer_fd, stride, ret;

      kms_b->gbm_buffer = gbm_bo_create(gbm_dev, width, height,
                                        GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT);

      buffer_fd = gbm_bo_get_fd(kms_b->gbm_buffer);
      stride = gbm_bo_get_stride(kms_b->gbm_buffer);


      VkDmaBufImageCreateInfo dmaBufInfo = {};

      VkExtent3D extent = {};
      extent.width = width;
      extent.height = height;
      extent.depth = 1;

      dmaBufInfo.sType = (VkStructureType) VK_STRUCTURE_TYPE_DMA_BUF_IMAGE_CREATE_INFO_INTEL;
      dmaBufInfo.fd = buffer_fd;
      dmaBufInfo.format = image_format;
      dmaBufInfo.extent = extent;
      dmaBufInfo.strideInBytes = stride;

      create_dma_buf_image(device,
                           &dmaBufInfo,
                           NULL,
                           &kms_b->mem,
                           &b->image);
      close(buffer_fd);

      kms_b->stride = gbm_bo_get_stride(kms_b->gbm_buffer);
      uint32_t bo_handles[4] = { (uint32_t) (gbm_bo_get_handle(kms_b->gbm_buffer).s32), };
      uint32_t pitches[4] = { (uint32_t) stride, };
      uint32_t offsets[4] = { 0, };
      ret = drmModeAddFB2(fd, width, height,
                          DRM_FORMAT_XRGB8888, bo_handles,
                          pitches, offsets, &kms_b->fb, 0);
      vik_log_f_if(ret == -1, "addfb2 failed");

      create_image_view(device, b->image,
                        image_format, &b->view);
    }
  }

  void set_mode_and_page_flip(int fd, drmModeCrtc *crtc, drmModeConnector *connector) {
    int ret = drmModeSetCrtc(fd, crtc->crtc_id, kms_buffers[0].fb,
        0, 0, &connector->connector_id, 1, &crtc->mode);
    vik_log_f_if(ret < 0, "modeset failed: %m");

    ret = drmModePageFlip(fd, crtc->crtc_id, kms_buffers[0].fb,
        DRM_MODE_PAGE_FLIP_EVENT, NULL);
    vik_log_f_if(ret < 0, "pageflip failed: %m");
  }

  void render(int fd, uint32_t crtc_id) {
    int index = current & 1;

    render_cb(index);
    KMSBuffer *kms_b = &kms_buffers[index];
    int ret = drmModePageFlip(fd, crtc_id, kms_b->fb,
                              DRM_MODE_PAGE_FLIP_EVENT, NULL);
    vik_log_f_if(ret < 0, "pageflip failed: %m");
    current++;
  }
};
}  // namespace vik
