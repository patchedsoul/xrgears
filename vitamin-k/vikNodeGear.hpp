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

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <vulkan/vulkan.h>

#include <vector>

#include "../vks/vksTools.hpp"
#include "../vks/vksBuffer.hpp"
#include "../vks/vksCamera.hpp"

#include "vikMaterial.hpp"
#include "vikGear.hpp"
#include "vikSkyBox.hpp"
#include "vikNode.hpp"

class VikNodeGear : public VikNode {
 private:
  VikGear gear;

 public:
  void generate(vks::Device *vulkanDevice, GearInfo *gearinfo, VkQueue queue) {
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
                       sizeof(Material::PushBlock), &info.material);

    vkCmdDrawIndexed(cmdbuffer, gear.indexCount, 1, 0, 0, 1);
  }
};

