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

#include "../input/vikInput.hpp"

namespace vik {

struct StereoView {
  glm::mat4 view[2];
};

class Camera {

public:
 Buffer uniformBuffer;

 std::function<void()> view_updated_cb = [](){};

 void set_view_updated_cb(std::function<void()> cb) {
   view_updated_cb = cb;
 }

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

 void init_ubo(Device *vulkanDevice) {
   vulkanDevice->create_and_map(&uniformBuffer, sizeof(ubo));
 }

 private:
  float fov;
  float znear, zfar;

  virtual void updateViewMatrix() = 0;
 public:
  enum CameraType { lookat, firstperson };
  CameraType type = CameraType::lookat;


  float rotationSpeed = 1.0f;

  glm::vec3 cameraPos = glm::vec3();
  glm::vec2 last_mouse_position;

  struct {
    bool left = false;
    bool right = false;
    bool middle = false;
  } mouseButtons;


  glm::vec3 rotation = glm::vec3();
  glm::vec3 position = glm::vec3();

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

  virtual bool moving() {
    return false;
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

  virtual void update_movement(float deltaTime) {}
  virtual void keyboard_key_cb(Input::Key key, bool state) {}
  virtual void pointer_axis_cb(Input::MouseScrollAxis axis, double value) {}
  virtual void pointer_motion_cb(double x, double y) {}

  void pointer_button_cb(Input::MouseButton button, bool state) {
    switch (button) {
      case Input::MouseButton::Left:
        mouseButtons.left = state;
        break;
      case Input::MouseButton::Middle:
        mouseButtons.middle = state;
        break;
      case Input::MouseButton::Right:
        mouseButtons.right = state;
        break;
    }
  }

};
}  // namespace vik
