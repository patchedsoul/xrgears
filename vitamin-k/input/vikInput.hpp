/*
 * vitamin-k
 *
 * Copyright (C) 2017 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This code is licensed under the GNU General Public License Version 3 (GPLv3)
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
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
    ESCAPE,
    SPACE,
    F1,
    W,
    A,
    S,
    D,
    P
  };
};
}  // namespace vik
