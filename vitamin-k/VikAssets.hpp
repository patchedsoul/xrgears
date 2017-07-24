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
