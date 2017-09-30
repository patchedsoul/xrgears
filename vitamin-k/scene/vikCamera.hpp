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

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gli/gli.hpp>

#include "vikCameraBase.hpp"

namespace vik {
class Camera {
 public:
  Buffer uniformBuffer;

  struct UBOCamera {
    glm::mat4 projection[2];
    glm::mat4 view[2];
    glm::mat4 skyView[2];
    glm::vec3 position;
  } uboCamera;

  virtual ~Camera() {
    uniformBuffer.destroy();
  }

  virtual void update(CameraBase camera) {
    uboCamera.projection[0] = camera.matrices.perspective;
    uboCamera.view[0] = camera.matrices.view;
    uboCamera.skyView[0] = glm::mat4(glm::mat3(camera.matrices.view));
    uboCamera.position = camera.position * -1.0f;
    memcpy(uniformBuffer.mapped, &uboCamera, sizeof(uboCamera));
  }

  void prepareUniformBuffers(Device *vulkanDevice) {
    vulkanDevice->create_and_map(&uniformBuffer, sizeof(uboCamera));
  }
};
}  // namespace vik
