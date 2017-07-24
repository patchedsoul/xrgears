#pragma once

#include <vulkan/vulkan.h>

#include "../vks/vksTools.hpp"

#include "VikAssets.hpp"

class VikShader {
public:
  static VkPipelineShaderStageCreateInfo load(VkDevice& device, std::string fileName, VkShaderStageFlagBits stage)
  {
    VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = stage;

    std::string path = VikAssets::getShaderPath() + fileName;
    shaderStage.module = vks::tools::loadShader(path.c_str(), device);
    shaderStage.pName = "main"; // todo : make param
    assert(shaderStage.module != VK_NULL_HANDLE);
    return shaderStage;
  }
};

