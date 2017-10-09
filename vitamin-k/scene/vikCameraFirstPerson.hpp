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

#include "vikCamera.hpp"

namespace vik {
class CameraFirstPerson : public Camera {
 public:
  explicit CameraFirstPerson() {}

  void update_view() {
    glm::mat4 rotM = glm::mat4();
    glm::mat4 transM;

    rotM = glm::rotate(rotM, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    transM = glm::translate(glm::mat4(), position);

    matrices.view = rotM * transM;
  }

  bool moving() {
    return keys.left || keys.right || keys.up || keys.down;
  }

  void update_movement(float deltaTime) {
    if (moving()) {
      glm::vec3 camFront;
      camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
      camFront.y = sin(glm::radians(rotation.x));
      camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
      camFront = glm::normalize(camFront);

      float moveSpeed = deltaTime * movement_speed;

      if (keys.up)
        position += camFront * moveSpeed;
      if (keys.down)
        position -= camFront * moveSpeed;
      if (keys.left)
        position -= glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
      if (keys.right)
        position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;

      update_view();
    }
  }

  void keyboard_key_cb(Input::Key key, bool state) {
    switch (key) {
      case Input::Key::W:
        keys.up = state;
        break;
      case Input::Key::S:
        keys.down = state;
        break;
      case Input::Key::A:
        keys.left = state;
        break;
      case Input::Key::D:
        keys.right = state;
        break;
      default:
        break;
    }
  }

  void pointer_motion_cb(double x, double y) {
    double dx = last_mouse_position.x - x;
    double dy = last_mouse_position.y - y;

    if (mouse_buttons.left) {
      //rotation.x += dy * 1.25f * rotationSpeed;
      //rotation.y -= dx * 1.25f * rotationSpeed;


      rotate(glm::vec3(
                      dy * rotation_speed,
                      -dx * rotation_speed,
                      0.0f));
      view_updated_cb();
    }

    last_mouse_position = glm::vec2(x, y);
  }

  void update_ubo() {


  }
};
}  // namespace vik
