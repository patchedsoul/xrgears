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

#include <vulkan/vulkan.h>

#include <string>

#include "vikTools.hpp"

#include "../system/vikAssets.hpp"

namespace vik {
class Shader {
 public:
  static VkPipelineShaderStageCreateInfo load(const VkDevice& device, std::string fileName, VkShaderStageFlagBits stage) {
    VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = stage;

    std::string path = Assets::getShaderPath() + fileName;
    shaderStage.module = tools::loadShader(path.c_str(), device);
    shaderStage.pName = "main";
    assert(shaderStage.module != VK_NULL_HANDLE);
    return shaderStage;
  }
};
}  // namespace vik
