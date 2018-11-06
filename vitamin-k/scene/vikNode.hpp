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
#include <glm/gtc/matrix_inverse.hpp>

#include "../render/vikModel.hpp"

#include "vikMaterial.hpp"
#include "../system/vikAssets.hpp"
#include "vikSkyBox.hpp"
#include "vikCamera.hpp"

namespace vik {
class Node {
 public:
  struct UBO {
    glm::mat4 normal[2];
    glm::mat4 model;
  } ubo;

  VkDescriptorSet descriptor_set;

  struct NodeInfo {
    glm::vec3 position;
    float rotation_speed;
    float rotation_offset;
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
    info.position = p;
  }

  void setInfo(NodeInfo *nodeinfo) {
    info.position = nodeinfo->position;
    info.rotation_offset = nodeinfo->rotation_offset;
    info.rotation_speed = nodeinfo->rotation_speed;
    info.material = nodeinfo->material;
  }

  void create_descriptor_set(const VkDevice& device,
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

    vik_log_check(vkAllocateDescriptorSets(device, &allocInfo, &descriptor_set));

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
      // Binding 0 : Vertex shader uniform buffer
      initializers::writeDescriptorSet(
      descriptor_set,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      0,
      &uniformBuffer.descriptor),
      initializers::writeDescriptorSet(
      descriptor_set,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      1,
      lightsDescriptor),
      initializers::writeDescriptorSet(
      descriptor_set,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      2,
      cameraDescriptor)
    };

    if (skyDome != nullptr)
      writeDescriptorSets.push_back(skyDome->get_cube_map_write_descriptor_set(3, descriptor_set));

    vkUpdateDescriptorSets(device,
                           static_cast<uint32_t>(writeDescriptorSets.size()),
                           writeDescriptorSets.data(),
                           0,
                           nullptr);
  }

  void update_uniform_buffer(Camera::StereoView sv, float timer) {
    ubo.model = glm::mat4();

    ubo.model = glm::translate(ubo.model, info.position);
    float rotation_z = (info.rotation_speed * timer * 360.0f) + info.rotation_offset;
    ubo.model = glm::rotate(ubo.model, glm::radians(rotation_z), glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.normal[0] = glm::inverseTranspose(sv.view[0] * ubo.model);
    ubo.normal[1] = glm::inverseTranspose(sv.view[1] * ubo.model);
    memcpy(uniformBuffer.mapped, &ubo, sizeof(ubo));
  }

  void init_uniform_buffer(Device *vulkanDevice) {
    vulkanDevice->create_and_map(&uniformBuffer, sizeof(ubo));
  }

  virtual void draw(VkCommandBuffer cmdbuffer, VkPipelineLayout pipelineLayout) {}
};
}  // namespace vik
