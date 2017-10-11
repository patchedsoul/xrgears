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

  void load_model(const std::string& name, VertexLayout layout,
                 float scale,  Device *device, VkQueue queue) {
    model.loadFromFile(vik::Assets::get_asset_path() + "models/" + name,
                       layout,
                       scale,
                       device,
                       queue);
  }

  void draw(VkCommandBuffer command_buffer, VkPipelineLayout pipeline_layout) {
    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &model.vertices.buffer, offsets);
    vkCmdBindIndexBuffer(command_buffer, model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdPushConstants(command_buffer,
                       pipeline_layout,
                       VK_SHADER_STAGE_FRAGMENT_BIT,
                       sizeof(glm::vec3),
                       sizeof(Material::PushBlock), &info.material);
    vkCmdDrawIndexed(command_buffer, model.indexCount, 1, 0, 0, 0);
  }
};
}  // namespace vik
