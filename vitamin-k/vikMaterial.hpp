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
