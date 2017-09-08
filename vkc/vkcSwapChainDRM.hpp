#pragma once

#include <gbm.h>
#include <vulkan/vulkan_intel.h>

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

#include "vkcSwapChain.hpp"

namespace vkc {

struct kms_buffer {
  gbm_bo *gbm_buffer;
  VkDeviceMemory mem;
  uint32_t fb;
  uint32_t stride;
};

class SwapChainDRM : public SwapChain {
public:

  kms_buffer kms_buffers[MAX_NUM_IMAGES];

  SwapChainDRM() {
  }

  ~SwapChainDRM() {
  }

  void init(VkDevice device, VkFormat image_format, gbm_device *gbm_dev, int fd,
            uint32_t width, uint32_t height, VkRenderPass render_pass) {
    PFN_vkCreateDmaBufImageINTEL create_dma_buf_image =
        (PFN_vkCreateDmaBufImageINTEL)vkGetDeviceProcAddr(device, "vkCreateDmaBufImageINTEL");

    for (uint32_t i = 0; i < 2; i++) {
      struct RenderBuffer *b = &buffers[i];
      struct kms_buffer *kms_b = &kms_buffers[i];
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

      init_buffer(device, image_format, render_pass,
                  width, height, b);
    }
  }
};
}
