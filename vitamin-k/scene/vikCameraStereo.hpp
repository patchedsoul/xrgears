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

#include "vikCameraFirstPerson.hpp"

namespace vik {
class CameraStereo : public CameraFirstPerson {
 public:
  // Camera and view properties
  float eyeSeparation = 0.08f;
  const float focalLength = 0.5f;

  int width, height;

  CameraStereo(int w, int h) : width(w), height(h) {
    fov = 90.0f;
    znear = 0.1f;
    zfar = 256.0f;
  }

  void changeEyeSeparation(float delta) {
    eyeSeparation += delta;
  }

  void update_ubo() {
    // Geometry shader matrices for the two viewports
    // See http://paulbourke.net/stereographics/stereorender/

    // Calculate some variables
    float aspectRatio = (float)(width * 0.5f) / (float)height;
    float wd2 = znear * tan(glm::radians(fov / 2.0f));
    float ndfl = znear / focalLength;
    float left, right;
    float top = wd2;
    float bottom = -wd2;

    glm::vec3 camFront;
    camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
    camFront.y = sin(glm::radians(rotation.x));
    camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
    camFront = glm::normalize(camFront);
    glm::vec3 camRight = glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::mat4 rotM = glm::mat4();
    glm::mat4 transM;

    rotM = glm::rotate(rotM, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    // Left eye
    left = -aspectRatio * wd2 + 0.5f * eyeSeparation * ndfl;
    right = aspectRatio * wd2 + 0.5f * eyeSeparation * ndfl;

    transM = glm::translate(glm::mat4(), position - camRight * (eyeSeparation / 2.0f));

    ubo.projection[0] = glm::frustum(left, right, bottom, top, znear, zfar);
    ubo.view[0] = rotM * transM;
    ubo.skyView[0] = rotM * glm::translate(glm::mat4(), -camRight * (eyeSeparation / 2.0f));

    // Right eye
    left = -aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;
    right = aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;

    transM = glm::translate(glm::mat4(), position + camRight * (eyeSeparation / 2.0f));

    ubo.projection[1] = glm::frustum(left, right, bottom, top, znear, zfar);
    ubo.view[1] = rotM * transM;
    ubo.skyView[1] = rotM * glm::translate(glm::mat4(), camRight * (eyeSeparation / 2.0f));

    ubo.position = position * -1.0f;

    memcpy(uniformBuffer.mapped, &ubo, sizeof(ubo));
  }
};
}  // namespace vik
