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

#include "../vks/vksBuffer.hpp"
#include "../vks/vksDevice.hpp"

class VikBuffer {
 public:
  static void create(vks::Device *vulkanDevice, vks::Buffer *buffer,
                     VkDeviceSize size) {
    vik_log_check(
          vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            buffer,
            size));

    // Map persistent
    vik_log_check(buffer->map());
  }
};
