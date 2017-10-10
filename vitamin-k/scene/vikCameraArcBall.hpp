/*
 * vitamin-k
 *
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "vikCamera.hpp"

namespace vik {
class CameraArcBall : public Camera {
 public:
  CameraArcBall() {
    rotation_speed = 1.25f;
  }

  float zoomSpeed = 1.0f;
  float zoom = 0;

  void update_view() {}

  void pointer_axis_cb(Input::MouseScrollAxis axis, double value) {
    switch (axis) {
      case Input::MouseScrollAxis::X:
        zoom += value * 0.005f * zoomSpeed;
        translate(glm::vec3(0.0f, 0.0f, value * 0.005f * zoomSpeed));
        view_updated_cb();
        break;
      default:
        break;
    }
  }

  void pointer_motion_cb(double x, double y) {
    double dx = last_mouse_position.x - x;
    double dy = last_mouse_position.y - y;

    if (mouse_buttons.left) {
      rotate(glm::vec3(dy * rotation_speed, -dx * rotation_speed, 0.0f));
      view_updated_cb();
    }

    if (mouse_buttons.right) {
      zoom += dy * .005f * zoomSpeed;
      view_updated_cb();
    }

    if (mouse_buttons.middle) {
      translate(glm::vec3(-dx * 0.01f, -dy * 0.01f, 0.0f));
      view_updated_cb();
    }
    last_mouse_position = glm::vec2(x, y);
  }

  glm::mat4 get_view_matrix() {
    return glm::translate(glm::mat4(), position)
        * glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom))
        * get_rotation_matrix();
  }

  const glm::vec3 X_AXIS = glm::vec3(1.0f, 0.0f, 0.0f);
  const glm::vec3 Y_AXIS = glm::vec3(0.0f, 1.0f, 0.0f);
  const glm::vec3 Z_AXIS = glm::vec3(0.0f, 0.0f, 1.0f);

  glm::mat4 get_rotation_matrix() {
    glm::mat4 matrix = glm::mat4();
    matrix = glm::rotate(matrix, glm::radians(rotation.x), X_AXIS);
    matrix = glm::rotate(matrix, glm::radians(rotation.y), Y_AXIS);
    matrix = glm::rotate(matrix, glm::radians(rotation.z), Z_AXIS);
    return matrix;
  }

  void update_ubo() {}
};
}  // namespace vik
