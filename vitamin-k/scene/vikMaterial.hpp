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

#include <string>

namespace vik {
struct Material {
  // Parameter block used as push constant block
  struct PushBlock {
    float roughness;
    float metallic;
    float r, g, b;
  } params;
  std::string name;
  Material() {}
  Material(std::string n, glm::vec3 c, float r, float m) : name(n) {
    params.roughness = r;
    params.metallic = m;
    params.r = c.r;
    params.g = c.g;
    params.b = c.b;
  }
};
}  // namespace vik
