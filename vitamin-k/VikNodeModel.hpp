/*
 * XRGears
 *
 * Copyright (C) 2017 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 *
 * Based on Vulkan Examples written by Sascha Willems
 */

#pragma once

#include "VikNode.hpp"

class VikNodeModel : public VikNode {
  vks::Model model;

public:
  ~VikNodeModel() {
    model.destroy();
  }

  void loadModel(const std::string& name, vks::VertexLayout layout,
                 float scale,  vks::VulkanDevice *device, VkQueue queue) {
    model.loadFromFile(VikAssets::getAssetPath() + "models/" + name,
                       layout,
                       scale,
                       device,
                       queue);
  }

  void draw(VkCommandBuffer cmdbuffer, VkPipelineLayout pipelineLayout) {
    vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &model.vertices.buffer, offsets);
    vkCmdBindIndexBuffer(cmdbuffer, model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(cmdbuffer,
                       pipelineLayout,
                       VK_SHADER_STAGE_FRAGMENT_BIT,
                       sizeof(glm::vec3),
                       sizeof(Material::PushBlock), &info.material);
    vkCmdDrawIndexed(cmdbuffer, model.indexCount, 1, 0, 0, 0);
  }
};
