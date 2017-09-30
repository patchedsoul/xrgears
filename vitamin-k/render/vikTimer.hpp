/*
 * vitamin-k
 *
 * Copyright (C) 2016 Sascha Willems - www.saschawillems.de
 * Copyright (C) 2017 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This code is licensed under the GNU General Public License Version 3 (GPLv3)
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Based on Vulkan Examples written by Sascha Willems
 */

#pragma once

#include <chrono>

namespace vik {
class Timer {
 public:
  const double SECOND_IN_MILLI = 1000.0;

  double time_since_tick = 0.0;
  uint32_t frames_since_tick = 0;
  uint32_t frames_per_second = 0;
  std::chrono::time_point<std::chrono::high_resolution_clock> frame_time_start;

  /** @brief Last frame time measured using a high performance timer (if available) */
  double frame_time_seconds = 1.0;

  // Defines a frame rate independent timer value clamped from -1.0...1.0
  // For use in animations, rotations, etc.
  float animation_timer = 0.0f;
  // Multiplier for speeding up (or slowing down) the global timer
  float animation_timer_speed = 0.25f;

  bool animation_paused = false;

  Timer() {}
  ~Timer() {}

  bool tick_finnished() {
    return time_since_tick > SECOND_IN_MILLI;
  }

  void increment() {
    frames_since_tick++;
  }

  void start() {
    frame_time_start = std::chrono::high_resolution_clock::now();
  }

  void update_fps() {
    frames_per_second = frames_since_tick;
  }

  void reset() {
    time_since_tick = 0.0f;
    frames_since_tick = 0;
  }

  void update_animation_timer() {
    if (animation_paused)
      return;

    animation_timer += animation_timer_speed * frame_time_seconds;
    // Convert to clamped timer value
    if (animation_timer > 1.0)
      animation_timer -= 1.0f;
  }

  void toggle_animation_pause() {
    animation_paused = !animation_paused;
  }

  float update_frame_time() {
    auto frame_time_end = std::chrono::high_resolution_clock::now();
    auto frame_time_milli = std::chrono::duration<double, std::milli>(frame_time_end - frame_time_start).count();

    frame_time_seconds = frame_time_milli / 1000.0f;  // to second
    time_since_tick += frame_time_milli;
    return frame_time_seconds;
  }
};
}
