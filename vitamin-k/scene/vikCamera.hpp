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

class Camera {
 protected:
  float fov = 60.f;
  float znear = .001f;
  float zfar = 256.f;

 public:
  enum CameraType { lookat, firstperson };
  CameraType type = CameraType::lookat;

  float rotation_speed = 1.0f;
  float movement_speed = 1.0f;

  glm::vec2 last_mouse_position;
  glm::vec3 rotation = glm::vec3();
  glm::vec3 position = glm::vec3();

  struct {
    bool left = false;
    bool right = false;
    bool middle = false;
  } mouse_buttons;

  struct {
    glm::mat4 projection;
    glm::mat4 view;
  } matrices;

  struct {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
  } keys;

 public:
  Buffer uniform_buffer;

  struct StereoView {
    glm::mat4 view[2];
  };

  struct UBOCamera {
    glm::mat4 projection[2];
    glm::mat4 view[2];
    glm::mat4 sky_view[2];
    glm::vec3 position;
  } ubo;

  virtual ~Camera() {
    uniform_buffer.destroy();
  }

  virtual void update_movement(float deltaTime) {}
  virtual void keyboard_key_cb(Input::Key key, bool state) {}
  virtual void pointer_axis_cb(Input::MouseScrollAxis axis, double value) {}
  virtual void pointer_motion_cb(double x, double y) {}
  virtual void update_view() {}

  virtual glm::mat4 get_view_matrix() {
    return glm::mat4();
  }

  virtual glm::mat4 get_rotation_matrix() {
    return glm::mat4();
  }

  virtual void update_uniform_buffer() {
    ubo.projection[0] = matrices.projection;
    ubo.view[0] = matrices.view;
    ubo.sky_view[0] = glm::mat4(glm::mat3(matrices.view));
    ubo.position = position * -1.0f;
    memcpy(uniform_buffer.mapped, &ubo, sizeof(ubo));
  }

  void init_uniform_buffer(Device *device) {
    device->create_and_map(&uniform_buffer, sizeof(ubo));
  }

  std::function<void()> view_updated_cb = [](){};

  void set_view_updated_cb(std::function<void()> cb) {
    view_updated_cb = cb;
  }

  virtual bool moving() {
    return false;
  }

  void set_perspective(float fov, float aspect, float znear, float zfar) {
    this->fov = fov;
    this->znear = znear;
    this->zfar = zfar;
    matrices.projection = glm::perspective(glm::radians(fov), aspect, znear, zfar);
  }

  void update_aspect_ratio(float aspect) {
    matrices.projection = glm::perspective(glm::radians(fov), aspect, znear, zfar);
  }

  glm::mat4 get_projection_matrix() {
    return matrices.projection;
  }

  void set_position(glm::vec3 p) {
    position = p;
    update_view();
  }

  void set_rotation(glm::vec3 r) {
    rotation = r;
    update_view();
  }

  void rotate(glm::vec3 delta) {
    rotation += delta;
    update_view();
  }

  void translate(glm::vec3 delta) {
    position += delta;
    update_view();
  }

  void pointer_button_cb(Input::MouseButton button, bool state) {
    switch (button) {
      case Input::MouseButton::Left:
        mouse_buttons.left = state;
        break;
      case Input::MouseButton::Middle:
        mouse_buttons.middle = state;
        break;
      case Input::MouseButton::Right:
        mouse_buttons.right = state;
        break;
    }
  }
};
}  // namespace vik
