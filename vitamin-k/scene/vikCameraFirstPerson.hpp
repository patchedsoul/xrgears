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

  void update_ubo() {


  }
};
}  // namespace vik
