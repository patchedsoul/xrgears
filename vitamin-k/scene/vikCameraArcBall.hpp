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
  explicit CameraArcBall() {}

  float zoomSpeed = 1.0f;
  float zoom = 0;

  void updateViewMatrix() {}

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

    if (mouseButtons.left) {

      rotate(glm::vec3(
                      dy * 1.25f * rotationSpeed,
                      -dx * 1.25f * rotationSpeed,
                      0.0f));

      view_updated_cb();
    }

    if (mouseButtons.right) {
      zoom += dy * .005f * zoomSpeed;
      //translate(glm::vec3(-0.0f, 0.0f, dy * .005f * zoomSpeed));
      view_updated_cb();
    }

    if (mouseButtons.middle) {
      cameraPos.x -= dx * 0.01f;
      cameraPos.y -= dy * 0.01f;
      //translate(glm::vec3(-dx * 0.01f, -dy * 0.01f, 0.0f));
      view_updated_cb();
    }
    last_mouse_position = glm::vec2(x, y);
  }


  void update_ubo() {


  }
};
}  // namespace vik
