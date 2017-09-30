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

#include <string>

#include "vikNode.hpp"

namespace vik {
class NodeModel : public Node {
  Model model;

 public:
  ~NodeModel() {
    model.destroy();
  }

  void loadModel(const std::string& name, VertexLayout layout,
                 float scale,  Device *device, VkQueue queue) {
    model.loadFromFile(vik::Assets::getAssetPath() + "models/" + name,
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
}  // namespace vik
