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

#include "../vks/vksBuffer.hpp"
#include "../vks/vksCamera.hpp"

#include "VikCamera.hpp"
#include "VikBuffer.hpp"

class VikCameraStereo : public VikCamera {
public:
  // Camera and view properties
  float eyeSeparation = 0.08f;
  const float focalLength = 0.5f;
  const float fov = 90.0f;
  const float zNear = 0.1f;
  const float zFar = 256.0f;

  int width, height;

  VikCameraStereo(int w, int h) : width(w), height(h) {}

  void changeEyeSeparation(float delta) {
    eyeSeparation += delta;
  }

  void update(Camera camera) {
    // Geometry shader matrices for the two viewports
    // See http://paulbourke.net/stereographics/stereorender/

    // Calculate some variables
    float aspectRatio = (float)(width * 0.5f) / (float)height;
    float wd2 = zNear * tan(glm::radians(fov / 2.0f));
    float ndfl = zNear / focalLength;
    float left, right;
    float top = wd2;
    float bottom = -wd2;

    glm::vec3 camFront;
    glm::vec3 rotation = glm::vec3();
    camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
    camFront.y = sin(glm::radians(rotation.x));
    camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
    camFront = glm::normalize(camFront);
    glm::vec3 camRight = glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::mat4 rotM = glm::mat4();
    glm::mat4 transM;

    rotM = glm::rotate(rotM, glm::radians(camera.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(camera.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(camera.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    // Left eye
    left = -aspectRatio * wd2 + 0.5f * eyeSeparation * ndfl;
    right = aspectRatio * wd2 + 0.5f * eyeSeparation * ndfl;

    transM = glm::translate(glm::mat4(), camera.position - camRight * (eyeSeparation / 2.0f));

    uboCamera.projection[0] = glm::frustum(left, right, bottom, top, zNear, zFar);
    uboCamera.view[0] = rotM * transM;
    uboCamera.skyView[0] = rotM * glm::translate(glm::mat4(), -camRight * (eyeSeparation / 2.0f));

    // Right eye
    left = -aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;
    right = aspectRatio * wd2 - 0.5f * eyeSeparation * ndfl;

    transM = glm::translate(glm::mat4(), camera.position + camRight * (eyeSeparation / 2.0f));

    uboCamera.projection[1] = glm::frustum(left, right, bottom, top, zNear, zFar);
    uboCamera.view[1] = rotM * transM;
    uboCamera.skyView[1] = rotM * glm::translate(glm::mat4(), camRight * (eyeSeparation / 2.0f));

    uboCamera.position = camera.position * -1.0f;

    memcpy(uniformBuffer.mapped, &uboCamera, sizeof(uboCamera));
  }
};
