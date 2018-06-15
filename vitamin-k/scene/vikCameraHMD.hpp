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
#include "../input/vikHMD.hpp"

namespace vik {
class CameraHMD : public CameraFirstPerson {
 public:
  HMD* hmd;

  explicit CameraHMD(HMD* h) : hmd(h) {}

  static inline void
  fix_handedness(glm::mat4& m) {
    m[0][1] = -m[0][1];
    m[1][0] = -m[1][0];
    m[1][2] = -m[1][2];
    m[2][1] = -m[2][1];
  }

  void update_uniform_buffer() {
    glm::mat4 hmd_projection_left, hmd_projection_right;
    glm::mat4 hmd_view_left, hmd_view_right;

    hmd->get_transformation(&hmd_projection_left, &hmd_projection_right,
                           &hmd_view_left, &hmd_view_right);

    fix_handedness(hmd_view_left);
    fix_handedness(hmd_view_right);

    glm::mat4 translation_matrix = glm::translate(glm::mat4(), position);

    ubo.projection[0] = hmd_projection_left;
    ubo.view[0] = hmd_view_left * translation_matrix;
    ubo.sky_view[0] = hmd_view_left;

    ubo.projection[1] = hmd_projection_right;
    ubo.view[1] = hmd_view_right  * translation_matrix;
    ubo.sky_view[1] = hmd_view_right;

    ubo.position = position * -1.0f;

    memcpy(uniform_buffer.mapped, &ubo, sizeof(ubo));
  }
};
}  // namespace vik
