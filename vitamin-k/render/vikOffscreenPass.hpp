 /*
 * vitamin-k
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <array>

#include "vikDevice.hpp"

// Offscreen frame buffer properties
#define FB_DIM 2048

namespace vik {
class OffscreenPass {
 private:
  VkDevice device;

  // One sampler for the frame buffer color attachments
  VkSampler colorSampler;

  // Framebuffer for offscreen rendering
  struct FrameBufferAttachment {
    VkImage image;
    VkDeviceMemory mem;
    VkImageView view;
    VkFormat format;
  };

  struct FrameBuffer {
    uint32_t width, height;
    VkFramebuffer frameBuffer;
    FrameBufferAttachment diffuseColor;
    FrameBufferAttachment depth;
    VkRenderPass renderPass;
  } offScreenFrameBuf;

 public:
  explicit OffscreenPass(const VkDevice& d) {
    device = d;
  }

  ~OffscreenPass() {
    vkDestroySampler(device, colorSampler, nullptr);

    // Color attachments
    vkDestroyImageView(device, offScreenFrameBuf.diffuseColor.view, nullptr);
    vkDestroyImage(device, offScreenFrameBuf.diffuseColor.image, nullptr);
    vkFreeMemory(device, offScreenFrameBuf.diffuseColor.mem, nullptr);

    // Depth attachment
    vkDestroyImageView(device, offScreenFrameBuf.depth.view, nullptr);
    vkDestroyImage(device, offScreenFrameBuf.depth.image, nullptr);
    vkFreeMemory(device, offScreenFrameBuf.depth.mem, nullptr);

    vkDestroyFramebuffer(device, offScreenFrameBuf.frameBuffer, nullptr);

    vkDestroyRenderPass(device, offScreenFrameBuf.renderPass, nullptr);
  }

  // Create a frame buffer attachment
  void createAttachment(
      Device *vulkanDevice,
      VkFormat format,
      VkImageUsageFlagBits usage,
      FrameBufferAttachment *attachment) {
    attachment->format = format;

    VkImageCreateInfo image = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = {
        .width = offScreenFrameBuf.width,
        .height = offScreenFrameBuf.height,
        .depth = 1
      },
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL
    };

    image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

    VkMemoryRequirements memReqs;
    vik_log_check(vkCreateImage(device, &image, nullptr, &attachment->image));
    vkGetImageMemoryRequirements(device, attachment->image, &memReqs);

    VkMemoryAllocateInfo memAlloc = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memReqs.size,
      .memoryTypeIndex =
        vulkanDevice->getMemoryType(memReqs.memoryTypeBits,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    vik_log_check(vkAllocateMemory(device, &memAlloc, nullptr, &attachment->mem));
    vik_log_check(vkBindImageMemory(device, attachment->image, attachment->mem, 0));

    VkImageAspectFlags aspectMask = 0;
    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
      aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    } else if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    assert(aspectMask > 0);

    VkImageViewCreateInfo imageView = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = attachment->image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .subresourceRange = {
        .aspectMask = aspectMask,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      },
    };

    vik_log_check(vkCreateImageView(device, &imageView, nullptr, &attachment->view));
  }

  // Prepare a new framebuffer and attachments for offscreen rendering (G-Buffer)
  void init_offscreen_framebuffer(Device *vulkanDevice, const VkPhysicalDevice& physicalDevice) {
    offScreenFrameBuf.width = FB_DIM;
    offScreenFrameBuf.height = FB_DIM;

    // Color attachments
    // (World space) Positions
    createAttachment(
          vulkanDevice,
          VK_FORMAT_R16G16B16A16_SFLOAT,
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
          &offScreenFrameBuf.diffuseColor);

    // Depth attachment
    // Find a suitable depth format
    VkFormat attDepthFormat;
    VkBool32 validDepthFormat = tools::getSupportedDepthFormat(physicalDevice, &attDepthFormat);
    assert(validDepthFormat);

    createAttachment(
          vulkanDevice,
          attDepthFormat,
          VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
          &offScreenFrameBuf.depth);

    // Set up separate renderpass with references to the color and depth attachments
    std::array<VkAttachmentDescription, 2> attachmentDescs = {};

    // Init attachment properties
    for (uint32_t i = 0; i < 2; ++i) {
      attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
      attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      if (i == 1) {
        attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      } else {
        attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      }
    }

    // Formats
    attachmentDescs[0].format = offScreenFrameBuf.diffuseColor.format;
    attachmentDescs[1].format = offScreenFrameBuf.depth.format;

    std::vector<VkAttachmentReference> colorReferences;
    colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

    VkAttachmentReference depthReference = {
      .attachment = 1,
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = static_cast<uint32_t>(colorReferences.size()),
      .pColorAttachments = colorReferences.data(),
      .pDepthStencilAttachment = &depthReference
    };

    // Use subpass dependencies for attachment layput transitions
    std::array<VkSubpassDependency, 2> dependencies = {
      (VkSubpassDependency) {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
      },
      (VkSubpassDependency)  {
        .srcSubpass = 0,
        .dstSubpass = VK_SUBPASS_EXTERNAL,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
      }
    };

    VkRenderPassCreateInfo renderPassInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = static_cast<uint32_t>(attachmentDescs.size()),
      .pAttachments = attachmentDescs.data(),
      .subpassCount = 1,
      .pSubpasses = &subpass,
      .dependencyCount = 2,
      .pDependencies = dependencies.data()
    };

    vik_log_check(vkCreateRenderPass(device, &renderPassInfo,
                                     nullptr, &offScreenFrameBuf.renderPass));

    std::array<VkImageView, 2> attachments;
    attachments[0] = offScreenFrameBuf.diffuseColor.view;
    attachments[1] = offScreenFrameBuf.depth.view;

    VkFramebufferCreateInfo fbufCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = offScreenFrameBuf.renderPass,
      .attachmentCount = static_cast<uint32_t>(attachments.size()),
      .pAttachments = attachments.data(),
      .width = offScreenFrameBuf.width,
      .height = offScreenFrameBuf.height,
      .layers = 1
    };

    vik_log_check(vkCreateFramebuffer(device, &fbufCreateInfo,
                                      nullptr, &offScreenFrameBuf.frameBuffer));

    // Create sampler to sample from the color attachments

    VkSamplerCreateInfo sampler = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .magFilter = VK_FILTER_NEAREST,
      .minFilter = VK_FILTER_NEAREST,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0f,
      .maxAnisotropy = 1.0f,
      .minLod = 0.0f,
      .maxLod = 1.0f,
      .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
    };

    vik_log_check(vkCreateSampler(device, &sampler, nullptr, &colorSampler));
  }

  VkDescriptorImageInfo get_descriptor_image_info() {
    // Image descriptors for the offscreen color attachments
    VkDescriptorImageInfo descriptor_info = {
          .sampler = colorSampler,
          .imageView = offScreenFrameBuf.diffuseColor.view,
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
    return descriptor_info;
  }

  VkWriteDescriptorSet
  get_image_write_descriptor_set(const VkDescriptorSet& descriptorSet,
                                 VkDescriptorImageInfo *texDescriptorPosition,
                                 uint32_t binding) {
    return  (VkWriteDescriptorSet) {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSet,
      .dstBinding = binding,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .pImageInfo = texDescriptorPosition
    };
  }

  void beginRenderPass(const VkCommandBuffer& cmdBuffer) {
    // Clear values for all attachments written in the fragment sahder
    std::array<VkClearValue, 2> clearValues;
    clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass =  offScreenFrameBuf.renderPass,
      .framebuffer = offScreenFrameBuf.frameBuffer,
      .renderArea = {
        .extent = {
          .width = offScreenFrameBuf.width,
          .height = offScreenFrameBuf.height
        }
      },
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues = clearValues.data()
    };

    vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo,
                         VK_SUBPASS_CONTENTS_INLINE);
  }

  void setViewPortAndScissor(const VkCommandBuffer& cmdBuffer) {
    VkViewport viewport = {
      .width = (float)offScreenFrameBuf.width,
      .height = (float)offScreenFrameBuf.height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f
    };
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor = {
      .offset = { .x = 0, .y = 0 },
      .extent = {
        .width = offScreenFrameBuf.width,
        .height = offScreenFrameBuf.height
      }
    };
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
  }

  void setViewPortAndScissorStereo(const VkCommandBuffer& cmdBuffer) {
    VkViewport viewports[2];

    uint32_t w = offScreenFrameBuf.width, h = offScreenFrameBuf.height;

    // Left
    viewports[0] = { 0, 0, (float) w / 2.0f, (float) h, 0.0, 1.0f };
    // Right
    viewports[1] = { (float) w / 2.0f, 0, (float) w / 2.0f, (float) h, 0.0, 1.0f };

    vkCmdSetViewport(cmdBuffer, 0, 2, viewports);

    VkRect2D scissorRects[2] = {
      {
        .offset = { .x = 0, .y = 0 },
        .extent = { .width = w / 2, .height = h }
      },
      {
        .offset = { .x = (int32_t) w / 2, .y = 0 },
        .extent = { .width = w / 2, .height = h }
      },
    };
    vkCmdSetScissor(cmdBuffer, 0, 2, scissorRects);
  }

  VkRenderPass getRenderPass() {
    return offScreenFrameBuf.renderPass;
  }
};
}  // namespace vik
