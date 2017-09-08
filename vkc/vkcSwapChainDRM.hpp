#pragma once

#include <gbm.h>

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
};
}
