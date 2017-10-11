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

#include "../render/vikTexture.hpp"
#include "../render/vikModel.hpp"

#include "../system/vikAssets.hpp"
#include "../render/vikShader.hpp"

namespace vik {
class SkyBox {
 private:
  TextureCubeMap cube_map;
  VkDescriptorSet descriptor_set;
  VkDevice device;
  VkDescriptorImageInfo texture_descriptor;
  Model model;
  VkPipeline pipeline;

 public:
  explicit SkyBox(VkDevice device) : device(device) {}

  ~SkyBox() {
    cube_map.destroy();
    model.destroy();
    vkDestroyPipeline(device, pipeline, nullptr);
  }

  void init_texture_descriptor() {
    // Image descriptor for the cube map texture
    texture_descriptor =
        initializers::descriptorImageInfo(
          cube_map.sampler,
          cube_map.view,
          cube_map.imageLayout);
  }

  VkWriteDescriptorSet get_cube_map_write_descriptor_set(unsigned binding, VkDescriptorSet ds) {
    return initializers::writeDescriptorSet(
          ds,
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          binding,
          &texture_descriptor);
  }

  void load_assets(VertexLayout vertexLayout, Device *vulkanDevice, VkQueue queue) {
    // Skybox
    model.loadFromFile(vik::Assets::get_asset_path() + "models/cube.obj", vertexLayout, 10.0f, vulkanDevice, queue);
    cube_map.loadFromFile(
          vik::Assets::get_texture_path() + "cubemap_yokohama_bc3_unorm.ktx", VK_FORMAT_BC2_UNORM_BLOCK,
          // VikAssets::getTexturePath() + "equirect/cube2/cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT,
          // VikAssets::getTexturePath() + "hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT,
          // VikAssets::getAssetPath() + "textures/cubemap_space.ktx", VK_FORMAT_R8G8B8A8_UNORM,
          vulkanDevice,
          queue);
    init_texture_descriptor();
  }

  void create_descriptor_set(const VkDescriptorSetAllocateInfo& allocInfo,
                           VkDescriptorBufferInfo* cameraDescriptor) {
    vik_log_check(vkAllocateDescriptorSets(device, &allocInfo, &descriptor_set));

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
      initializers::writeDescriptorSet(
      descriptor_set,
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      2,
      cameraDescriptor)
    };

    writeDescriptorSets.push_back(get_cube_map_write_descriptor_set(3, descriptor_set));

    vkUpdateDescriptorSets(device,
                           static_cast<uint32_t>(writeDescriptorSets.size()),
                           writeDescriptorSets.data(),
                           0,
                           nullptr);
  }

  void draw(VkCommandBuffer cmdbuffer, VkPipelineLayout pipelineLayout) {
    VkDeviceSize offsets[1] = { 0 };

    vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1, &descriptor_set, 0, nullptr);
    vkCmdBindVertexBuffers(cmdbuffer, 0, 1, &model.vertices.buffer, offsets);
    vkCmdBindIndexBuffer(cmdbuffer, model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdDrawIndexed(cmdbuffer, model.indexCount, 1, 0, 0, 0);
  }

  void init_pipeline(VkGraphicsPipelineCreateInfo* pipeline_info,
                      const VkPipelineCache& pipeline_cache) {
    VkPipelineRasterizationStateCreateInfo rasterization_state =
        initializers::pipelineRasterizationStateCreateInfo(
          VK_POLYGON_MODE_FILL,
          VK_CULL_MODE_BACK_BIT,
          VK_FRONT_FACE_COUNTER_CLOCKWISE,
          0);

    // Skybox pipeline (background cube)
    std::array<VkPipelineShaderStageCreateInfo, 3> shader_stages;

    shader_stages[0] = vik::Shader::load(device, "xrgears/sky.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shader_stages[1] = vik::Shader::load(device, "xrgears/sky.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    shader_stages[2] = vik::Shader::load(device, "xrgears/sky.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT);

    pipeline_info->stageCount = shader_stages.size();
    pipeline_info->pStages = shader_stages.data();
    pipeline_info->pRasterizationState = &rasterization_state;

    vik_log_check(vkCreateGraphicsPipelines(device, pipeline_cache, 1,
                                            pipeline_info, nullptr, &pipeline));

    vkDestroyShaderModule(device, shader_stages[0].module, nullptr);
    vkDestroyShaderModule(device, shader_stages[1].module, nullptr);
    vkDestroyShaderModule(device, shader_stages[2].module, nullptr);
  }
};
}  // namespace vik
