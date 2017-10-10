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
    shaderStage.module = load(path.c_str(), device);
    shaderStage.pName = "main";
    assert(shaderStage.module != VK_NULL_HANDLE);
    return shaderStage;
  }

  static VkShaderModule load(const char *fileName, VkDevice device) {
    std::ifstream is(fileName, std::ios::binary | std::ios::in | std::ios::ate);

    if (is.is_open()) {
      size_t size = is.tellg();
      is.seekg(0, std::ios::beg);
      char* shaderCode = new char[size];
      is.read(shaderCode, size);
      is.close();

      assert(size > 0);

      VkShaderModule shaderModule;
      VkShaderModuleCreateInfo moduleCreateInfo{};
      moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
      moduleCreateInfo.codeSize = size;
      moduleCreateInfo.pCode = (uint32_t*)shaderCode;

      vik_log_check(vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule));

      // vik_log_d("Creating shader module %p (%s)", shaderModule, fileName);

      delete[] shaderCode;

      return shaderModule;
    } else {
      std::cerr << "Error: Could not open shader file \"" << fileName << "\"" << std::endl;
      return VK_NULL_HANDLE;
    }
  }
};
}  // namespace vik
