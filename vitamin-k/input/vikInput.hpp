/*
 * vitamin-k
 *
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace vik {
class Input {
 public:
  enum MouseButton {
    Left,
    Middle,
    Right
  };

  enum MouseScrollAxis {
    X,
    Y
  };

  enum Key {
    UNKNOWN,
    ESCAPE,
    SPACE,
    KPPLUS,
    KPMINUS,
    F1,
    W,
    A,
    S,
    D,
    P
  };
};
}  // namespace vik
