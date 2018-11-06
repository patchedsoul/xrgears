/*
* Vulkan texture loader
*
* Copyright(C) 2016-2017 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license(MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <stdlib.h>

#include <vulkan/vulkan.h>
#include <gli/gli.hpp>

#include <string>
#include <fstream>
#include <vector>

#include "vikTools.hpp"
#include "vikDevice.hpp"
#include "vikBuffer.hpp"

#include "../system/vikLog.hpp"

namespace vik {
/** @brief Vulkan texture base class */
class Texture {
 public:
  Device *device;
  VkImage image;
  VkImageLayout imageLayout;
  VkDeviceMemory deviceMemory;
  VkImageView view;
  uint32_t width, height;
  uint32_t mipLevels;
  uint32_t layerCount;
  VkDescriptorImageInfo descriptor;

  /** @brief Optional sampler to use with this texture */
  VkSampler sampler;

  /** @brief Update image descriptor from current sampler, view and image layout */
  void updateDescriptor() {
    descriptor.sampler = sampler;
    descriptor.imageView = view;
    descriptor.imageLayout = imageLayout;
  }

  /** @brief Release all Vulkan resources held by this texture */
  void destroy() {
    vkDestroyImageView(device->logicalDevice, view, nullptr);
    vkDestroyImage(device->logicalDevice, image, nullptr);
    if (sampler)
      vkDestroySampler(device->logicalDevice, sampler, nullptr);
    vkFreeMemory(device->logicalDevice, deviceMemory, nullptr);
  }
};

/** @brief 2D texture */
class Texture2D : public Texture {
 public:
  /**
    * Load a 2D texture including all mip levels
    *
    * @param filename File to load (supports .ktx and .dds)
    * @param format Vulkan format of the image data stored in the file
    * @param device Vulkan device to create the texture on
    * @param copyQueue Queue used for the texture staging copy commands (must support transfer)
    * @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
    * @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    * @param (Optional) forceLinear Force linear tiling (not advised, defaults to false)
    *
    */
  void loadFromFile(
      std::string filename,
      VkFormat format,
      Device *device,
      VkQueue copyQueue,
      VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
      VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      bool forceLinear = false) {
    bool exists = tools::fileExists(filename);
    vik_log_f_if(!exists, "File not found: Could not load texture from %s", filename.c_str());

    gli::texture2d tex2D(gli::load(filename.c_str()));

    assert(!tex2D.empty());

    this->device = device;
    width = static_cast<uint32_t>(tex2D[0].extent().x);
    height = static_cast<uint32_t>(tex2D[0].extent().y);
    mipLevels = static_cast<uint32_t>(tex2D.levels());

    // Get device properites for the requested texture format
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &formatProperties);

    // Only use linear tiling if requested (and supported by the device)
    // Support for linear tiling is mostly limited, so prefer to use
    // optimal tiling instead
    // On most implementations linear tiling will only support a very
    // limited amount of formats and features (mip maps, cubemaps, arrays, etc.)
    VkBool32 useStaging = !forceLinear;

    VkMemoryAllocateInfo memAllocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    };
    VkMemoryRequirements memReqs;

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    if (useStaging) {
      // Create a host-visible staging buffer that contains the raw image data
      VkBuffer stagingBuffer;
      VkDeviceMemory stagingMemory;

      VkBufferCreateInfo bufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = tex2D.size(),
        // This buffer is used as a transfer source for the buffer copy
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
      };

      vik_log_check(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

      // Get memory requirements for the staging buffer (alignment, memory type bits)
      vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

      memAllocInfo.allocationSize = memReqs.size;
      // Get memory type index for a host visible buffer
      memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      vik_log_check(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
      vik_log_check(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

      // Copy texture data into staging buffer
      uint8_t *data;
      vik_log_check(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
      memcpy(data, tex2D.data(), tex2D.size());
      vkUnmapMemory(device->logicalDevice, stagingMemory);

      // Setup buffer copy regions for each mip level
      std::vector<VkBufferImageCopy> bufferCopyRegions;
      uint32_t offset = 0;

      for (uint32_t i = 0; i < mipLevels; i++) {
        VkBufferImageCopy bufferCopyRegion = {
          .bufferOffset = offset,
          .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = i,
            .baseArrayLayer = 0,
            .layerCount = 1,
          },
          .imageExtent = {
            .width = static_cast<uint32_t>(tex2D[i].extent().x),
            .height = static_cast<uint32_t>(tex2D[i].extent().y),
            .depth = 1,
          }
        };

        bufferCopyRegions.push_back(bufferCopyRegion);

        offset += static_cast<uint32_t>(tex2D[i].size());
      }

      // Create optimal tiled target image
      VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {
          .width = width,
          .height = height,
          .depth = 1
        },
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = imageUsageFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
      };

      // Ensure that the TRANSFER_DST bit is set for staging
      if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
        imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
      vik_log_check(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

      vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

      memAllocInfo.allocationSize = memReqs.size;

      memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      vik_log_check(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
      vik_log_check(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

      VkImageSubresourceRange subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = mipLevels,
        .layerCount = 1
      };

      // Image barrier for optimal image (target)
      // Optimal image will be used as destination for the copy
      tools::setImageLayout(
            copyCmd,
            image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

      // Copy mip levels from staging buffer
      vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(bufferCopyRegions.size()),
            bufferCopyRegions.data());

      // Change texture image layout to shader read after all mip levels have been copied
      this->imageLayout = imageLayout;
      tools::setImageLayout(
            copyCmd,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            imageLayout,
            subresourceRange);

      device->flushCommandBuffer(copyCmd, copyQueue);

      // Clean up staging resources
      vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
      vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
    } else {
      // Prefer using optimal tiling, as linear tiling
      // may support only a small set of features
      // depending on implementation (e.g. no mip maps, only one layer, etc.)

      // Check if this support is supported for linear tiling
      assert(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);

      VkImage mappableImage;
      VkDeviceMemory mappableMemory;

      VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {
          .width = width,
          .height = height,
          .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = imageUsageFlags,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
      };

      // Load mip map level 0 to linear tiling image
      vik_log_check(vkCreateImage(device->logicalDevice, &imageCreateInfo,
                                  nullptr, &mappableImage));

      // Get memory requirements for this image
      // like size and alignment
      vkGetImageMemoryRequirements(device->logicalDevice, mappableImage, &memReqs);
      // Set memory allocation size to required memory size
      memAllocInfo.allocationSize = memReqs.size;

      // Get memory type that can be mapped to host memory
      memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

      // Allocate host memory
      vik_log_check(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &mappableMemory));

      // Bind allocated image for use
      vik_log_check(vkBindImageMemory(device->logicalDevice, mappableImage, mappableMemory, 0));

      // Get sub resource layout
      // Mip map count, array layer, etc.
      VkImageSubresource subRes = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0
      };

      VkSubresourceLayout subResLayout;
      void *data;

      // Get sub resources layout
      // Includes row pitch, size offsets, etc.
      vkGetImageSubresourceLayout(device->logicalDevice, mappableImage, &subRes, &subResLayout);

      // Map image memory
      vik_log_check(vkMapMemory(device->logicalDevice, mappableMemory, 0, memReqs.size, 0, &data));

      // Copy image data into memory
      memcpy(data, tex2D[subRes.mipLevel].data(), tex2D[subRes.mipLevel].size());

      vkUnmapMemory(device->logicalDevice, mappableMemory);

      // Linear tiled images don't need to be staged
      // and can be directly used as textures
      image = mappableImage;
      deviceMemory = mappableMemory;
      this->imageLayout = imageLayout;

      // Setup image memory barrier
      tools::setImageLayout(copyCmd, image, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_UNDEFINED, imageLayout);

      device->flushCommandBuffer(copyCmd, copyQueue);
    }

    // Create a defaultsampler
    VkSamplerCreateInfo samplerCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_TRUE,
      .maxAnisotropy = 8,
      .compareOp = VK_COMPARE_OP_NEVER,
      .minLod = 0.0f,
      // Max level-of-detail should match mip level count
      .maxLod = (useStaging) ? (float)mipLevels : 0.0f,
      // Enable anisotropic filtering
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
    };

    vik_log_check(vkCreateSampler(device->logicalDevice, &samplerCreateInfo,
                                  nullptr, &sampler));

    // Create image view
    // Textures are not directly accessed by the shaders and
    // are abstracted by image views containing additional
    // information and sub resource ranges
    VkImageViewCreateInfo viewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components = {
        .r = VK_COMPONENT_SWIZZLE_R,
        .g = VK_COMPONENT_SWIZZLE_G,
        .b = VK_COMPONENT_SWIZZLE_B,
        .a = VK_COMPONENT_SWIZZLE_A
      },
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        // Linear tiling usually won't support mip maps
        // Only set mip map count if optimal tiling is used
        .levelCount = (useStaging) ? mipLevels : 1,
        .baseArrayLayer = 0,
        .layerCount = 1
      }
    };

    vik_log_check(vkCreateImageView(device->logicalDevice, &viewCreateInfo,
                                    nullptr, &view));

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
  }

  /**
    * Creates a 2D texture from a buffer
    *
    * @param buffer Buffer containing texture data to upload
    * @param bufferSize Size of the buffer in machine units
    * @param width Width of the texture to create
    * @param height Height of the texture to create
    * @param format Vulkan format of the image data stored in the file
    * @param device Vulkan device to create the texture on
    * @param copyQueue Queue used for the texture staging copy commands (must support transfer)
    * @param (Optional) filter Texture filtering for the sampler (defaults to VK_FILTER_LINEAR)
    * @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
    * @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    */
  void fromBuffer(
      void* buffer,
      VkDeviceSize bufferSize,
      VkFormat format,
      uint32_t width,
      uint32_t height,
      Device *device,
      VkQueue copyQueue,
      VkFilter filter = VK_FILTER_LINEAR,
      VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
      VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    assert(buffer);

    this->device = device;
    this->width = width;
    this->height = height;
    mipLevels = 1;

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = bufferSize,
      // This buffer is used as a transfer source for the buffer copy
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    vik_log_check(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo,
                                 nullptr, &stagingBuffer));

    VkMemoryRequirements memReqs;
    // Get memory requirements for the staging buffer (alignment, memory type bits)
    vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo memAllocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memReqs.size,
      // Get memory type index for a host visible buffer
      .memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    vik_log_check(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
    vik_log_check(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

    // Copy texture data into staging buffer
    uint8_t *data;
    vik_log_check(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
    memcpy(data, buffer, bufferSize);
    vkUnmapMemory(device->logicalDevice, stagingMemory);

    VkBufferImageCopy bufferCopyRegion = {
      .bufferOffset = 0,
      .imageSubresource = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
      .imageExtent = {
        .width = width,
        .height = height,
        .depth = 1,
      }
    };

    // Create optimal tiled target image
    VkImageCreateInfo imageCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = {
        .width = width,
        .height = height,
        .depth = 1
      },
      .mipLevels = mipLevels,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = imageUsageFlags,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    // Ensure that the TRANSFER_DST bit is set for staging
    if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
      imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    vik_log_check(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

    vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;

    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vik_log_check(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
    vik_log_check(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

    VkImageSubresourceRange subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = mipLevels,
      .layerCount = 1
    };

    // Image barrier for optimal image (target)
    // Optimal image will be used as destination for the copy
    tools::setImageLayout(
          copyCmd,
          image,
          VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          subresourceRange);

    // Copy mip levels from staging buffer
    vkCmdCopyBufferToImage(
          copyCmd,
          stagingBuffer,
          image,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          1,
          &bufferCopyRegion);

    // Change texture image layout to shader read after all mip levels have been copied
    this->imageLayout = imageLayout;
    tools::setImageLayout(
          copyCmd,
          image,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          imageLayout,
          subresourceRange);

    device->flushCommandBuffer(copyCmd, copyQueue);

    // Clean up staging resources
    vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
    vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = filter,
      .minFilter = filter,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
      .mipLodBias = 0.0f,
      .compareOp = VK_COMPARE_OP_NEVER,
      .minLod = 0.0f,
      .maxLod = 0.0f
    };

    vik_log_check(vkCreateSampler(device->logicalDevice, &samplerCreateInfo, nullptr, &sampler));

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components = {
        .r = VK_COMPONENT_SWIZZLE_R,
        .g = VK_COMPONENT_SWIZZLE_G,
        .b = VK_COMPONENT_SWIZZLE_B,
        .a = VK_COMPONENT_SWIZZLE_A
      },
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
      }
    };

    vik_log_check(vkCreateImageView(device->logicalDevice, &viewCreateInfo,
                                    nullptr, &view));

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
  }
};

/** @brief 2D array texture */
class Texture2DArray : public Texture {
 public:
  /**
    * Load a 2D texture array including all mip levels
    *
    * @param filename File to load (supports .ktx and .dds)
    * @param format Vulkan format of the image data stored in the file
    * @param device Vulkan device to create the texture on
    * @param copyQueue Queue used for the texture staging copy commands (must support transfer)
    * @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
    * @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    *
    */
  void loadFromFile(
      std::string filename,
      VkFormat format,
      Device *device,
      VkQueue copyQueue,
      VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
      VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    vik_log_f_if(!tools::fileExists(filename),
                 "File not found: Could not load texture from %s",
                 filename.c_str());

    gli::texture2d_array tex2DArray(gli::load(filename));

    assert(!tex2DArray.empty());

    this->device = device;
    width = static_cast<uint32_t>(tex2DArray.extent().x);
    height = static_cast<uint32_t>(tex2DArray.extent().y);
    layerCount = static_cast<uint32_t>(tex2DArray.layers());
    mipLevels = static_cast<uint32_t>(tex2DArray.levels());

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = tex2DArray.size(),
      // This buffer is used as a transfer source for the buffer copy
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    vik_log_check(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

    // Get memory requirements for the staging buffer (alignment, memory type bits)
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo memAllocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memReqs.size,
      // Get memory type index for a host visible buffer
      .memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    vik_log_check(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
    vik_log_check(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

    // Copy texture data into staging buffer
    uint8_t *data;
    vik_log_check(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
    memcpy(data, tex2DArray.data(), static_cast<size_t>(tex2DArray.size()));
    vkUnmapMemory(device->logicalDevice, stagingMemory);

    // Setup buffer copy regions for each layer including all of it's miplevels
    std::vector<VkBufferImageCopy> bufferCopyRegions;
    size_t offset = 0;

    for (uint32_t layer = 0; layer < layerCount; layer++) {
      for (uint32_t level = 0; level < mipLevels; level++) {
        VkBufferImageCopy bufferCopyRegion = {
          .bufferOffset = offset,
          .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = level,
            .baseArrayLayer = layer,
            .layerCount = 1
          },
          .imageExtent = {
            .width = static_cast<uint32_t>(tex2DArray[layer][level].extent().x),
            .height = static_cast<uint32_t>(tex2DArray[layer][level].extent().y),
            .depth = 1,
          }
        };
        bufferCopyRegions.push_back(bufferCopyRegion);

        // Increase offset into staging buffer for next level / face
        offset += tex2DArray[layer][level].size();
      }
    }

    // Create optimal tiled target image
    VkImageCreateInfo imageCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = {
        .width = width,
        .height = height,
        .depth = 1
      },
      .mipLevels = mipLevels,
      .arrayLayers = layerCount,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = imageUsageFlags,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    // Ensure that the TRANSFER_DST bit is set for staging
    if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
      imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    vik_log_check(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &image));

    vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vik_log_check(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
    vik_log_check(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Image barrier for optimal image (target)
    // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
    VkImageSubresourceRange subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = mipLevels,
      .layerCount = layerCount
    };

    tools::setImageLayout(
          copyCmd,
          image,
          VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          subresourceRange);

    // Copy the layers and mip levels from the staging buffer to the optimal tiled image
    vkCmdCopyBufferToImage(
          copyCmd,
          stagingBuffer,
          image,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          static_cast<uint32_t>(bufferCopyRegions.size()),
          bufferCopyRegions.data());

    // Change texture image layout to shader read after all faces have been copied
    this->imageLayout = imageLayout;
    tools::setImageLayout(
          copyCmd,
          image,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          imageLayout,
          subresourceRange);

    device->flushCommandBuffer(copyCmd, copyQueue);

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0f,
      .maxAnisotropy = 8,
      .compareOp = VK_COMPARE_OP_NEVER,
      .minLod = 0.0f,
      .maxLod = (float)mipLevels,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
    };

    vik_log_check(vkCreateSampler(device->logicalDevice, &samplerCreateInfo,
                                  nullptr, &sampler));

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
      .format = format,
      .components = {
        .r = VK_COMPONENT_SWIZZLE_R,
        .g = VK_COMPONENT_SWIZZLE_G,
        .b = VK_COMPONENT_SWIZZLE_B,
        .a = VK_COMPONENT_SWIZZLE_A
      },
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = mipLevels,
        .baseArrayLayer = 0,
        .layerCount = layerCount
      }
    };

    vik_log_check(vkCreateImageView(device->logicalDevice, &viewCreateInfo,
                                    nullptr, &view));

    // Clean up staging resources
    vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
    vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
  }
};

/** @brief Cube map texture */
class TextureCubeMap : public Texture {
 public:
  /**
    * Load a cubemap texture including all mip levels from a single file
    *
    * @param filename File to load (supports .ktx and .dds)
    * @param format Vulkan format of the image data stored in the file
    * @param device Vulkan device to create the texture on
    * @param copyQueue Queue used for the texture staging copy commands (must support transfer)
    * @param (Optional) imageUsageFlags Usage flags for the texture's image (defaults to VK_IMAGE_USAGE_SAMPLED_BIT)
    * @param (Optional) imageLayout Usage layout for the texture (defaults VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    *
    */
  void loadFromFile(
      std::string filename,
      VkFormat format,
      Device *device,
      VkQueue copyQueue,
      VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
      VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    vik_log_f_if(!tools::fileExists(filename),
                 "File not found: Could not load texture from %s",
                 filename.c_str());

    gli::texture_cube texCube(gli::load(filename));

    assert(!texCube.empty());

    this->device = device;
    width = static_cast<uint32_t>(texCube.extent().x);
    height = static_cast<uint32_t>(texCube.extent().y);
    mipLevels = static_cast<uint32_t>(texCube.levels());

    // Create a host-visible staging buffer that contains the raw image data
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = texCube.size(),
      // This buffer is used as a transfer source for the buffer copy
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    vik_log_check(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

    // Get memory requirements for the staging buffer (alignment, memory type bits)
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo memAllocInfo = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memReqs.size,
      // Get memory type index for a host visible buffer
      .memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    vik_log_check(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
    vik_log_check(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

    // Copy texture data into staging buffer
    uint8_t *data;
    vik_log_check(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data));
    memcpy(data, texCube.data(), texCube.size());
    vkUnmapMemory(device->logicalDevice, stagingMemory);

    // Setup buffer copy regions for each face including all of it's miplevels
    std::vector<VkBufferImageCopy> bufferCopyRegions;
    size_t offset = 0;

    for (uint32_t face = 0; face < 6; face++) {
      for (uint32_t level = 0; level < mipLevels; level++) {
        VkBufferImageCopy bufferCopyRegion = {
          .bufferOffset = offset,
          .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = level,
            .baseArrayLayer = face,
            .layerCount = 1
          },
          .imageExtent = {
            .width = static_cast<uint32_t>(texCube[face][level].extent().x),
            .height = static_cast<uint32_t>(texCube[face][level].extent().y),
            .depth = 1
          }
        };
        bufferCopyRegions.push_back(bufferCopyRegion);

        // Increase offset into staging buffer for next level / face
        offset += texCube[face][level].size();
      }
    }

    // Create optimal tiled target image
    VkImageCreateInfo imageCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      // This flag is required for cube map images
      .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = {
        .width = width,
        .height = height,
        .depth = 1
      },
      .mipLevels = mipLevels,
      // Cube faces count as array layers in Vulkan
      .arrayLayers = 6,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = imageUsageFlags,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    // Ensure that the TRANSFER_DST bit is set for staging
    if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
      imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    vik_log_check(vkCreateImage(device->logicalDevice, &imageCreateInfo,
                                nullptr, &image));

    vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);

    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vik_log_check(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &deviceMemory));
    vik_log_check(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));

    // Use a separate command buffer for texture loading
    VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // Image barrier for optimal image (target)
    // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
    VkImageSubresourceRange subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = mipLevels,
      .layerCount = 6
    };

    tools::setImageLayout(
          copyCmd,
          image,
          VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          subresourceRange);

    // Copy the cube map faces from the staging buffer to the optimal tiled image
    vkCmdCopyBufferToImage(
          copyCmd,
          stagingBuffer,
          image,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          static_cast<uint32_t>(bufferCopyRegions.size()),
          bufferCopyRegions.data());

    // Change texture image layout to shader read after all faces have been copied
    this->imageLayout = imageLayout;
    tools::setImageLayout(
          copyCmd,
          image,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          imageLayout,
          subresourceRange);

    device->flushCommandBuffer(copyCmd, copyQueue);

    // Create sampler
    VkSamplerCreateInfo samplerCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0f,
      .maxAnisotropy = 8,
      .compareOp = VK_COMPARE_OP_NEVER,
      .minLod = 0.0f,
      .maxLod = (float)mipLevels,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
    };

    vik_log_check(vkCreateSampler(device->logicalDevice, &samplerCreateInfo,
                                  nullptr, &sampler));

    // Create image view
    VkImageViewCreateInfo viewCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
      .format = format,
      .components = {
        .r = VK_COMPONENT_SWIZZLE_R,
        .g = VK_COMPONENT_SWIZZLE_G,
        .b = VK_COMPONENT_SWIZZLE_B,
        .a = VK_COMPONENT_SWIZZLE_A
      },
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = mipLevels,
        .baseArrayLayer = 0,
        .layerCount = 6
      }
    };

    vik_log_check(vkCreateImageView(device->logicalDevice, &viewCreateInfo,
                                    nullptr, &view));

    // Clean up staging resources
    vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);
    vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);

    // Update descriptor image info member that can be used for setting up descriptor sets
    updateDescriptor();
  }
};
}  // namespace vik
