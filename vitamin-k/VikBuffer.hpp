#pragma once

#include "VulkanBuffer.hpp"
#include "VulkanDevice.hpp"

class VikBuffer {
public:
    static void create(vks::VulkanDevice *vulkanDevice, vks::Buffer *buffer,  VkDeviceSize size) {
      VK_CHECK_RESULT(vulkanDevice->createBuffer(
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        buffer,
                        size));

      // Map persistent
      VK_CHECK_RESULT(buffer->map());
    }
};
