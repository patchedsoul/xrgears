/*
* Vulkan Example - Animated gears using multiple uniform buffers
*
* See readme.md for details
*
* Copyright (C) 2015 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <math.h>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "vulkan/vulkan.h"

#include "VulkanTools.h"

#include "VulkanBuffer.hpp"
#include "camera.hpp"
#include "uniformbuffers.h"
#include "Gear.h"

struct GearNodeInfo
{
  glm::vec3 pos;
  float rotSpeed;
  float rotOffset;
  Material material;
};

class GearNode
{
private:

  Gear gear;

  struct UBO
  {
    glm::mat4 normal[2];
    glm::mat4 model;
  };
  UBO ubo;

  vks::VulkanDevice *vulkanDevice;

  glm::vec3 pos;
  float rotSpeed;
  float rotOffset;

public:
  Material material;

  VkDescriptorSet descriptorSet;
  vks::Buffer uniformBuffer;

  //void setupDescriptorSet(VkDescriptorPool pool, VkDescriptorSetLayout descriptorSetLayout, std::vector<VkWriteDescriptorSet> *writeDescriptorSets);
  //glm::mat4 getModelMatrix(glm::vec3 rotation, float timer);

  GearNode(vks::VulkanDevice *vulkanDevice) : vulkanDevice(vulkanDevice) {}

  ~GearNode() {
    uniformBuffer.destroy();
  }

  void generate(GearNodeInfo *gearNodeinfo, GearInfo *gearinfo, VkQueue queue) {
    //	this->color = gearinfo->color;
    this->pos = gearNodeinfo->pos;
    this->rotOffset = gearNodeinfo->rotOffset;
    this->rotSpeed = gearNodeinfo->rotSpeed;
    this->material = gearNodeinfo->material;

    gear.generate(vulkanDevice, gearinfo, queue);
  }

  void draw(VkCommandBuffer cmdbuffer, VkPipelineLayout pipelineLayout) {
    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
    vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &gear.vertexBuffer.buffer, offsets);
    vkCmdBindIndexBuffer(cmdbuffer, gear.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdPushConstants(cmdbuffer,
                       pipelineLayout,
                       VK_SHADER_STAGE_FRAGMENT_BIT,
                       sizeof(glm::vec3),
                       sizeof(Material::PushBlock), &material);

    vkCmdDrawIndexed(cmdbuffer, gear.indexCount, 1, 0, 0, 1);
  }

  void updateUniformBuffer(StereoView sv, float timer) {
    ubo.model = glm::mat4();

    ubo.model = glm::translate(ubo.model, pos);
    float rotation_z = (rotSpeed * timer * 360.0f) + rotOffset;
    ubo.model = glm::rotate(ubo.model, glm::radians(rotation_z), glm::vec3(0.0f, 0.0f, 1.0f));

    /*
    */
    ubo.normal[0] = glm::inverseTranspose(sv.view[0] * ubo.model);
    ubo.normal[1] = glm::inverseTranspose(sv.view[1] * ubo.model);
    memcpy(uniformBuffer.mapped, &ubo, sizeof(ubo));
  }

  void prepareUniformBuffer() {
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &uniformBuffer,
                      sizeof(ubo)));
    // Map persistent
    VK_CHECK_RESULT(uniformBuffer.map());
  }

};

