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
  static const std::string get_asset_path() {
    return "./data/";
  }

  static const std::string get_shader_path() {
    return get_asset_path() + "shaders/";
  }

  static const std::string get_texture_path() {
    return get_asset_path() + "textures/";
  }
};
}  // namespace vik
