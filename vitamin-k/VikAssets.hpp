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

#include <string>

class VikAssets {
public:

  static const std::string getAssetPath() {
    return "./data/";
  }

  static const std::string getShaderPath() {
    return getAssetPath() + "shaders/";
  }

  static const std::string getTexturePath() {
    return getAssetPath() + "textures/";
  }
};
