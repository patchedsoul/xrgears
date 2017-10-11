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
  float eye_separation = 0.08f;
  const float focal_length = 0.5f;

  int width, height;

  CameraStereo(int w, int h) : width(w), height(h) {
    fov = 90.0f;
    znear = 0.1f;
    zfar = 256.0f;
  }

  void change_eye_separation(float delta) {
    eye_separation += delta;
  }

  void update_uniform_buffer() {
    // Geometry shader matrices for the two viewports
    // See http://paulbourke.net/stereographics/stereorender/

    // Calculate some variables
    float aspect_ratio = (float)(width * 0.5f) / (float)height;
    float wd2 = znear * tan(glm::radians(fov / 2.0f));
    float ndfl = znear / focal_length;
    float left, right;
    float top = wd2;
    float bottom = -wd2;

    glm::vec3 front_vec;
    front_vec.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
    front_vec.y = sin(glm::radians(rotation.x));
    front_vec.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
    front_vec = glm::normalize(front_vec);
    glm::vec3 right_vec = glm::normalize(glm::cross(front_vec, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::mat4 rot_mat = glm::mat4();
    glm::mat4 trans_mat;

    rot_mat = glm::rotate(rot_mat, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rot_mat = glm::rotate(rot_mat, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rot_mat = glm::rotate(rot_mat, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    // Left eye
    left = -aspect_ratio * wd2 + 0.5f * eye_separation * ndfl;
    right = aspect_ratio * wd2 + 0.5f * eye_separation * ndfl;

    trans_mat = glm::translate(glm::mat4(), position - right_vec * (eye_separation / 2.0f));

    ubo.projection[0] = glm::frustum(left, right, bottom, top, znear, zfar);
    ubo.view[0] = rot_mat * trans_mat;
    ubo.sky_view[0] = rot_mat * glm::translate(glm::mat4(), -right_vec * (eye_separation / 2.0f));

    // Right eye
    left = -aspect_ratio * wd2 - 0.5f * eye_separation * ndfl;
    right = aspect_ratio * wd2 - 0.5f * eye_separation * ndfl;

    trans_mat = glm::translate(glm::mat4(), position + right_vec * (eye_separation / 2.0f));

    ubo.projection[1] = glm::frustum(left, right, bottom, top, znear, zfar);
    ubo.view[1] = rot_mat * trans_mat;
    ubo.sky_view[1] = rot_mat * glm::translate(glm::mat4(), right_vec * (eye_separation / 2.0f));

    ubo.position = position * -1.0f;

    memcpy(uniform_buffer.mapped, &ubo, sizeof(ubo));
  }
};
}  // namespace vik
