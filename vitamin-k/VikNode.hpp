#pragma once

#include "../vks/vksModel.hpp"

#include "VikMaterial.hpp"
#include "VikAssets.hpp"
#include "VikBuffer.hpp"


class VikNode {

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

  vks::Buffer uniformBuffer;

  VikNode() {
  }

  ~VikNode() {
    uniformBuffer.destroy();
  }

  void setMateral(Material& m) {
    info.material = m;
  }

  void setPosition(glm::vec3& p) {
    info.pos = p;
  }

  void setInfo(NodeInfo *nodeinfo) {
    info.pos = nodeinfo->pos;
    info.rotOffset = nodeinfo->rotOffset;
    info.rotSpeed = nodeinfo->rotSpeed;
    info.material = nodeinfo->material;
  }

  void createDescriptorSet(VkDevice& device,
                           VkDescriptorPool& descriptorPool,
                           VkDescriptorSetLayout& descriptorSetLayout,
                           VkDescriptorBufferInfo& lightsDescriptor,
                           VkDescriptorBufferInfo& cameraDescriptor,
                           VikSkyBox *skyDome) {
    VkDescriptorSetAllocateInfo allocInfo =
        vks::initializers::descriptorSetAllocateInfo(
          descriptorPool,
          &descriptorSetLayout,
          1);

    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {

      // Binding 0 : Vertex shader uniform buffer
      vks::initializers::writeDescriptorSet(
      descriptorSet,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      0,
      &uniformBuffer.descriptor),
      vks::initializers::writeDescriptorSet(
      descriptorSet,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      1,
      &lightsDescriptor),
      vks::initializers::writeDescriptorSet(
      descriptorSet,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      2,
      &cameraDescriptor)
    };

    if (skyDome != nullptr)
      writeDescriptorSets.push_back(skyDome->getCubeMapWriteDescriptorSet(3, descriptorSet));

    vkUpdateDescriptorSets(device,
                           static_cast<uint32_t>(writeDescriptorSets.size()),
                           writeDescriptorSets.data(),
                           0,
                           nullptr);
  }

  void updateUniformBuffer(StereoView sv, float timer) {
    ubo.model = glm::mat4();

    ubo.model = glm::translate(ubo.model, info.pos);
    float rotation_z = (info.rotSpeed * timer * 360.0f) + info.rotOffset;
    ubo.model = glm::rotate(ubo.model, glm::radians(rotation_z), glm::vec3(0.0f, 0.0f, 1.0f));

    ubo.normal[0] = glm::inverseTranspose(sv.view[0] * ubo.model);
    ubo.normal[1] = glm::inverseTranspose(sv.view[1] * ubo.model);
    memcpy(uniformBuffer.mapped, &ubo, sizeof(ubo));
  }

  void prepareUniformBuffer(vks::VulkanDevice *vulkanDevice) {
    VikBuffer::create(vulkanDevice, &uniformBuffer, sizeof(ubo));
  }
};
