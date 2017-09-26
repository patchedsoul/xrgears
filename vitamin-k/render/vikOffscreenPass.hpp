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

#include <vulkan/vulkan.h>

#include <vector>

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
    int32_t width, height;
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
    VkImageAspectFlags aspectMask = 0;
    VkImageLayout imageLayout;

    attachment->format = format;

    if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
      aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    assert(aspectMask > 0);

    VkImageCreateInfo image = initializers::imageCreateInfo();
    image.imageType = VK_IMAGE_TYPE_2D;
    image.format = format;
    image.extent.width = offScreenFrameBuf.width;
    image.extent.height = offScreenFrameBuf.height;
    image.extent.depth = 1;
    image.mipLevels = 1;
    image.arrayLayers = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;
    image.tiling = VK_IMAGE_TILING_OPTIMAL;
    image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

    VkMemoryAllocateInfo memAlloc = initializers::memoryAllocateInfo();
    VkMemoryRequirements memReqs;

    vik_log_check(vkCreateImage(device, &image, nullptr, &attachment->image));
    vkGetImageMemoryRequirements(device, attachment->image, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vik_log_check(vkAllocateMemory(device, &memAlloc, nullptr, &attachment->mem));
    vik_log_check(vkBindImageMemory(device, attachment->image, attachment->mem, 0));

    VkImageViewCreateInfo imageView = initializers::imageViewCreateInfo();
    imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageView.format = format;
    imageView.subresourceRange = {};
    imageView.subresourceRange.aspectMask = aspectMask;
    imageView.subresourceRange.baseMipLevel = 0;
    imageView.subresourceRange.levelCount = 1;
    imageView.subresourceRange.baseArrayLayer = 0;
    imageView.subresourceRange.layerCount = 1;
    imageView.image = attachment->image;
    vik_log_check(vkCreateImageView(device, &imageView, nullptr, &attachment->view));
  }

  // Prepare a new framebuffer and attachments for offscreen rendering (G-Buffer)
  void prepareOffscreenFramebuffer(Device *vulkanDevice, const VkPhysicalDevice& physicalDevice) {
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

    VkAttachmentReference depthReference = {};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pColorAttachments = colorReferences.data();
    subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
    subpass.pDepthStencilAttachment = &depthReference;

    // Use subpass dependencies for attachment layput transitions
    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.pAttachments = attachmentDescs.data();
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies.data();

    vik_log_check(vkCreateRenderPass(device, &renderPassInfo, nullptr, &offScreenFrameBuf.renderPass));

    std::array<VkImageView, 2> attachments;
    attachments[0] = offScreenFrameBuf.diffuseColor.view;
    attachments[1] = offScreenFrameBuf.depth.view;

    VkFramebufferCreateInfo fbufCreateInfo = {};
    fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufCreateInfo.pNext = NULL;
    fbufCreateInfo.renderPass = offScreenFrameBuf.renderPass;
    fbufCreateInfo.pAttachments = attachments.data();
    fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    fbufCreateInfo.width = offScreenFrameBuf.width;
    fbufCreateInfo.height = offScreenFrameBuf.height;
    fbufCreateInfo.layers = 1;
    vik_log_check(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offScreenFrameBuf.frameBuffer));

    // Create sampler to sample from the color attachments
    VkSamplerCreateInfo sampler = initializers::samplerCreateInfo();
    sampler.magFilter = VK_FILTER_NEAREST;
    sampler.minFilter = VK_FILTER_NEAREST;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV = sampler.addressModeU;
    sampler.addressModeW = sampler.addressModeU;
    sampler.mipLodBias = 0.0f;
    sampler.maxAnisotropy = 1.0;
    sampler.minLod = 0.0f;
    sampler.maxLod = 1.0f;
    sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    vik_log_check(vkCreateSampler(device, &sampler, nullptr, &colorSampler));
  }

  VkDescriptorImageInfo getDescriptorImageInfo() {
    // Image descriptors for the offscreen color attachments
    return initializers::descriptorImageInfo(
          colorSampler,
          offScreenFrameBuf.diffuseColor.view,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  VkWriteDescriptorSet getImageWriteDescriptorSet(const VkDescriptorSet& descriptorSet, VkDescriptorImageInfo *texDescriptorPosition, uint32_t binding) {
    return initializers::writeDescriptorSet(
          descriptorSet,
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          binding,
          texDescriptorPosition);
  }

  void beginRenderPass(const VkCommandBuffer& cmdBuffer) {
    // Clear values for all attachments written in the fragment sahder
    std::array<VkClearValue, 2> clearValues;
    clearValues[0].color = { { 1.0f, 1.0f, 1.0f, 1.0f } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass =  offScreenFrameBuf.renderPass;
    renderPassBeginInfo.framebuffer = offScreenFrameBuf.frameBuffer;
    renderPassBeginInfo.renderArea.extent.width = offScreenFrameBuf.width;
    renderPassBeginInfo.renderArea.extent.height = offScreenFrameBuf.height;
    renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

  void setViewPortAndScissor(const VkCommandBuffer& cmdBuffer) {
    VkViewport viewport =
        initializers::viewport(
          (float)offScreenFrameBuf.width,
          (float)offScreenFrameBuf.height,
          0.0f, 1.0f);
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor =
        initializers::rect2D(
          offScreenFrameBuf.width,
          offScreenFrameBuf.height,
          0, 0);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
  }

  void setViewPortAndScissorStereo(const VkCommandBuffer& cmdBuffer) {
    VkViewport viewports[2];

    int32_t w = offScreenFrameBuf.width, h = offScreenFrameBuf.height;

    // Left
    viewports[0] = { 0, 0, (float) w / 2.0f, (float) h, 0.0, 1.0f };
    // Right
    viewports[1] = { (float) w / 2.0f, 0, (float) w / 2.0f, (float) h, 0.0, 1.0f };

    vkCmdSetViewport(cmdBuffer, 0, 2, viewports);

    VkRect2D scissorRects[2] = {
      initializers::rect2D(w / 2, h, 0, 0),
      initializers::rect2D(w / 2, h, w / 2, 0),
    };
    vkCmdSetScissor(cmdBuffer, 0, 2, scissorRects);
  }

  VkRenderPass getRenderPass() {
    return offScreenFrameBuf.renderPass;
  }
};
}
