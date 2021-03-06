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

#include <vulkan/vulkan.h>

#include <string>

#include "vikWindowWayland.hpp"

namespace vik {
class WindowWaylandShell : public WindowWayland {
  wl_shell *shell = nullptr;
  wl_shell_surface *shell_surface = nullptr;

 public:
  explicit WindowWaylandShell(Settings *s) : WindowWayland(s) {
    name = "wayland-shell";
  }

  ~WindowWaylandShell() {
    wl_shell_surface_destroy(shell_surface);
    wl_shell_destroy(shell);
  }

  int init() {
    display = wl_display_connect(NULL);

    if (!display) {
      vik_log_e("Could not connect to Wayland display.");
      return -1;
    }

    wl_registry *registry = wl_display_get_registry(display);
    if (!registry) {
      vik_log_e("Could not get Wayland registry.");
      return -1;
    }

    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    if (!compositor || !shell || !seat) {
      vik_log_e("Could not bind Wayland protocols.");
      return -1;
    }

    wl_registry_destroy(registry);

    surface = wl_compositor_create_surface(compositor);
    shell_surface = wl_shell_get_shell_surface(shell, surface);

    wl_shell_surface_add_listener(shell_surface, &shell_surface_listener, this);
    // wl_shell_surface_set_toplevel(shell_surface);

    return 0;
  }

  void fullscreen() {
    fullscreen(current_display()->output, current_mode()->refresh);
  }

  void fullscreen(wl_output *output, int refresh) {
    wl_shell_surface_set_fullscreen(shell_surface,
                                    WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                    refresh,
                                    output);
  }

  void update_window_title(const std::string& title) {
    wl_shell_surface_set_title(shell_surface, title.c_str());
  }

  void registry_global(wl_registry *registry, uint32_t name,
                       const char *interface) {
    if (strcmp(interface, "wl_compositor") == 0) {
      compositor = (wl_compositor*)
          wl_registry_bind(registry, name, &wl_compositor_interface, 3);
    } else if (strcmp(interface, "wl_shell") == 0) {
      shell = (wl_shell*)
          wl_registry_bind(registry, name, &wl_shell_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
      seat = (wl_seat*)
          wl_registry_bind(registry, name, &wl_seat_interface, 1);
      wl_seat_add_listener(seat, &seat_listener, this);
    } else if (strcmp(interface, "wl_output") == 0) {
      wl_output* _output = (wl_output*)
          wl_registry_bind(registry, name, &wl_output_interface, 2);
      wl_output_add_listener(_output, &output_listener, this);
    }
  }

  static void _configure_cb(void *data, wl_shell_surface *shell_surface,
                            uint32_t edges, int32_t width, int32_t height) {
    WindowWaylandShell *self = reinterpret_cast<WindowWaylandShell *>(data);
    self->configure(width, height);
  }

  const wl_shell_surface_listener shell_surface_listener = {
    _ping_cb,
    _configure_cb,
    _popup_done_cb
  };

  static void _ping_cb(void *data, wl_shell_surface *shell_surface,
                       uint32_t serial) {
    wl_shell_surface_pong(shell_surface, serial);
  }

  // Unused callbacks
  static void _popup_done_cb(void *data, wl_shell_surface *shell_surface) {}
};
}  // namespace vik
