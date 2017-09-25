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

#include "../vks/vksCamera.hpp"

#include "vikCamera.hpp"
#include "vikBuffer.hpp"
#include "vikHMD.hpp"

namespace vik {
class CameraHMD : public Camera {
 public:
  VikHMD* hmd;

  explicit CameraHMD(VikHMD* h) : hmd(h) {}

  static inline void
  fix_handedness(glm::mat4& m) {
    m[0][1] = -m[0][1];
    m[1][0] = -m[1][0];
    m[1][2] = -m[1][2];
    m[2][1] = -m[2][1];
  }

  void update(vks::Camera camera) {
    glm::mat4 hmdProjectionLeft, hmdProjectionRight;
    glm::mat4 hmdViewLeft, hmdViewRight;

    hmd->getTransformation(&hmdProjectionLeft, &hmdProjectionRight,
                           &hmdViewLeft, &hmdViewRight);

    fix_handedness(hmdViewLeft);
    fix_handedness(hmdViewRight);

    glm::mat4 translationMatrix = glm::translate(glm::mat4(), camera.position);

    uboCamera.projection[0] = hmdProjectionLeft;
    uboCamera.view[0] = hmdViewLeft * translationMatrix;
    uboCamera.skyView[0] = hmdViewLeft;

    uboCamera.projection[1] = hmdProjectionRight;
    uboCamera.view[1] = hmdViewRight  * translationMatrix;
    uboCamera.skyView[1] = hmdViewRight;

    uboCamera.position = camera.position * -1.0f;

    memcpy(uniformBuffer.mapped, &uboCamera, sizeof(uboCamera));
  }
};
}
