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
  CameraFirstPerson() {}

  void update_view() {
    glm::mat4 rot_mat = glm::mat4();
    glm::mat4 trans_mat;

    rot_mat = glm::rotate(rot_mat, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rot_mat = glm::rotate(rot_mat, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rot_mat = glm::rotate(rot_mat, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    trans_mat = glm::translate(glm::mat4(), position);

    matrices.view = rot_mat * trans_mat;

    // vik_log_d("pos: %.2f %.2f %.2f", position.x, position.y, position.z);
    // vik_log_d("rot: %.2f %.2f %.2f", rotation.x, rotation.y, rotation.z);
  }

  bool moving() {
    return keys.left || keys.right || keys.up || keys.down;
  }

  const glm::vec3 up_vec = glm::vec3(0.0f, 1.0f, 0.0f);

  void update_movement(float time) {
    if (moving()) {
      float rad_x = glm::radians(rotation.x);
      float rad_y = glm::radians(rotation.y);

      glm::vec3 front_vec;
      front_vec.x = -cos(rad_x) * sin(rad_y);
      front_vec.y = sin(rad_x);
      front_vec.z = cos(rad_x) * cos(rad_y);
      front_vec = glm::normalize(front_vec);

      float move_distance = time * movement_speed;

      glm::vec3 side_vec = glm::normalize(glm::cross(front_vec, up_vec));

      if (keys.up)
        position += front_vec * move_distance;
      if (keys.down)
        position -= front_vec * move_distance;
      if (keys.left)
        position -= side_vec * move_distance;
      if (keys.right)
        position += side_vec * move_distance;

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
      rotate(glm::vec3(dy * rotation_speed, -dx * rotation_speed, 0.0f));
      view_updated_cb();
    }

    last_mouse_position = glm::vec2(x, y);
  }

  void update_uniform_buffer() {}
};
}  // namespace vik
