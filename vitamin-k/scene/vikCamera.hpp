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
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../render/vikBuffer.hpp"
#include "../render/vikDevice.hpp"

namespace vik {

struct StereoView {
  glm::mat4 view[2];
};

class Camera {

public:
 Buffer uniformBuffer;

 struct UBOCamera {
   glm::mat4 projection[2];
   glm::mat4 view[2];
   glm::mat4 skyView[2];
   glm::vec3 position;
 } ubo;

 virtual ~Camera() {
   uniformBuffer.destroy();
 }

 virtual void update_ubo() {
   ubo.projection[0] = matrices.perspective;
   ubo.view[0] = matrices.view;
   ubo.skyView[0] = glm::mat4(glm::mat3(matrices.view));
   ubo.position = position * -1.0f;
   memcpy(uniformBuffer.mapped, &ubo, sizeof(ubo));
 }

 void prepareUniformBuffers(Device *vulkanDevice) {
   vulkanDevice->create_and_map(&uniformBuffer, sizeof(ubo));
 }

 private:
  float fov;
  float znear, zfar;

  void updateViewMatrix() {
    glm::mat4 rotM = glm::mat4();
    glm::mat4 transM;

    rotM = glm::rotate(rotM, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    transM = glm::translate(glm::mat4(), position);

    if (type == CameraType::firstperson)
      matrices.view = rotM * transM;
    else
      matrices.view = transM * rotM;
  }

 public:
  enum CameraType { lookat, firstperson };
  CameraType type = CameraType::lookat;

  glm::vec3 rotation = glm::vec3();
  glm::vec3 position = glm::vec3();

  float rotationSpeed = 1.0f;
  float movementSpeed = 1.0f;

  struct {
    glm::mat4 perspective;
    glm::mat4 view;
  } matrices;

  struct {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
  } keys;

  bool moving() {
    return keys.left || keys.right || keys.up || keys.down;
  }

  void setPerspective(float fov, float aspect, float znear, float zfar) {
    this->fov = fov;
    this->znear = znear;
    this->zfar = zfar;
    matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
  }

  void update_aspect_ratio(float aspect) {
    matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
  }

  void setPosition(glm::vec3 position) {
    this->position = position;
    updateViewMatrix();
  }

  void setRotation(glm::vec3 rotation) {
    this->rotation = rotation;
    updateViewMatrix();
  }

  void rotate(glm::vec3 delta) {
    this->rotation += delta;
    updateViewMatrix();
  }

  void translate(glm::vec3 delta) {
    this->position += delta;
    vik_log_d("Translating");
    updateViewMatrix();
  }

  void update_movement(float deltaTime) {
    //if (type == CameraType::firstperson) {
      if (moving()) {
        glm::vec3 camFront;
        camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
        camFront.y = sin(glm::radians(rotation.x));
        camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
        camFront = glm::normalize(camFront);

        float moveSpeed = deltaTime * movementSpeed;

        if (keys.up)
          position += camFront * moveSpeed;
        if (keys.down)
          position -= camFront * moveSpeed;
        if (keys.left)
          position -= glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
        if (keys.right)
          position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;

        updateViewMatrix();
      }
    //}
  }
};
}  // namespace vik
