/*
 * vitamin-k
 *
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

namespace vik {
class Assets {
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
}  // namespace vik
