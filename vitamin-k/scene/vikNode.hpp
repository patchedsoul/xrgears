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

#include <vector>

#include "render/vikModel.hpp"

#include "vikMaterial.hpp"
#include "system/vikAssets.hpp"
#include "vikSkyBox.hpp"

namespace vik {
class Node {
 public:
  struct UBO {
    glm::mat4 normal[2];
    glm::mat4 model;
  } ubo;

  VkDescriptorSet descriptorSet;

  struct NodeInfo {
    glm::vec3 pos;
    float rotSpeed;
    float rotOffset;
    Material material;
  } info;

  Buffer uniformBuffer;

  Node() {
  }

  virtual ~Node() {
    uniformBuffer.destroy();
  }

  void setMateral(const Material& m) {
    info.material = m;
  }

  void setPosition(const glm::vec3& p) {
    info.pos = p;
  }

  void setInfo(NodeInfo *nodeinfo) {
    info.pos = nodeinfo->pos;
    info.rotOffset = nodeinfo->rotOffset;
    info.rotSpeed = nodeinfo->rotSpeed;
    info.material = nodeinfo->material;
  }

  void createDescriptorSet(const VkDevice& device,
                           const VkDescriptorPool& descriptorPool,
                           const VkDescriptorSetLayout& descriptorSetLayout,
                           VkDescriptorBufferInfo* lightsDescriptor,
                           VkDescriptorBufferInfo* cameraDescriptor,
                           vik::SkyBox *skyDome) {
    VkDescriptorSetAllocateInfo allocInfo =
        initializers::descriptorSetAllocateInfo(
          descriptorPool,
          &descriptorSetLayout,
          1);

    vik_log_check(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
      // Binding 0 : Vertex shader uniform buffer
      initializers::writeDescriptorSet(
      descriptorSet,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      0,
      &uniformBuffer.descriptor),
      initializers::writeDescriptorSet(
      descriptorSet,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      1,
      lightsDescriptor),
      initializers::writeDescriptorSet(
      descriptorSet,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      2,
      cameraDescriptor)
    };

    if (skyDome != nullptr)
      writeDescriptorSets.push_back(skyDome->getCubeMapWriteDescriptorSet(3, descriptorSet));

    vkUpdateDescriptorSets(device,
                           static_cast<uint32_t>(writeDescriptorSets.size()),
                           writeDescriptorSets.data(),
                           0,
                           nullptr);
  }

  void updateUniformBuffer(vik::Camera::StereoView sv, float timer) {
    ubo.model = glm::mat4();

    ubo.model = glm::translate(ubo.model, info.pos);
    float rotation_z = (info.rotSpeed * timer * 360.0f) + info.rotOffset;
    ubo.model = glm::rotate(ubo.model, glm::radians(rotation_z), glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.normal[0] = glm::inverseTranspose(sv.view[0] * ubo.model);
    ubo.normal[1] = glm::inverseTranspose(sv.view[1] * ubo.model);
    memcpy(uniformBuffer.mapped, &ubo, sizeof(ubo));
  }

  void prepareUniformBuffer(Device *vulkanDevice) {
    vulkanDevice->create_and_map(&uniformBuffer, sizeof(ubo));
  }

  virtual void draw(VkCommandBuffer cmdbuffer, VkPipelineLayout pipelineLayout) {}
};
}  // namespace vik
