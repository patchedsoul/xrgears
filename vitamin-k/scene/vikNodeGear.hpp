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

#include "../render/vikTools.hpp"
#include "../render/vikBuffer.hpp"
#include "vikCamera.hpp"

#include "vikMaterial.hpp"
#include "vikGear.hpp"
#include "vikSkyBox.hpp"
#include "vikNode.hpp"

namespace vik {
class NodeGear : public Node {
 private:
  Gear gear;

 public:
  void generate(Device *vik_device, GearInfo *gear_info, VkQueue queue) {
    gear.generate(vik_device, gear_info, queue);
  }

  void draw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout) {
    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &gear.vertexBuffer.buffer, offsets);
    vkCmdBindIndexBuffer(command_buffer, gear.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdPushConstants(command_buffer,
                       pipeline_layout,
                       VK_SHADER_STAGE_FRAGMENT_BIT,
                       sizeof(glm::vec3),
                       sizeof(Material::PushBlock), &info.material);

    vkCmdDrawIndexed(command_buffer, gear.indexCount, 1, 0, 0, 1);
  }
};
}  // namespace vik
