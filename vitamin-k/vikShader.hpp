/*
 * XRGears
 *
 * Copyright (C) 2016 Sascha Willems - www.saschawillems.de
 * Copyright (C) 2017 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 *
 * Based on Vulkan Examples written by Sascha Willems
 */

#pragma once

#include <vulkan/vulkan.h>

#include <string>

#include "vikTools.hpp"

#include "vikAssets.hpp"

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
}

