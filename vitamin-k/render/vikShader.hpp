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
    std::string path = Assets::get_shader_path() + fileName;
    VkShaderModule module = load(path.c_str(), device);
    assert(module != VK_NULL_HANDLE);

    VkPipelineShaderStageCreateInfo shaderStage = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = stage,
      .module = module,
      .pName = "main"
    };

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
      VkShaderModuleCreateInfo moduleCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = (uint32_t*)shaderCode
      };

      vik_log_check(vkCreateShaderModule(device, &moduleCreateInfo,
                                         NULL, &shaderModule));

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
