/*
 * vitamin-k
 *
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string.h>
#include <stdio.h>

#include <poll.h>

#include <vulkan/vulkan.h>
#include <string>

#include "../../xdg-shell/xdg-shell-unstable-v6-client-protocol.h"

#include "vikWindowWayland.hpp"
#include "../system/vikLog.hpp"

namespace vik {
class WindowWaylandXDG : public WindowWayland {
  zxdg_shell_v6 *shell = nullptr;
  zxdg_surface_v6 *xdg_surface = nullptr;
  zxdg_toplevel_v6 *xdg_toplevel = nullptr;
  SwapChainVkComplex swap_chain;

 public:
  explicit WindowWaylandXDG(Settings *s) : WindowWayland(s) {
    name = "wayland-xdg";
  }

  ~WindowWaylandXDG() {}

  // Return -1 on failure.
  int init() {
    display = wl_display_connect(NULL);
    if (!display)
      return -1;

    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);

    wl_display_roundtrip(display);

    wl_registry_destroy(registry);

    surface = wl_compositor_create_surface(compositor);

    vik_log_f_if(!shell,
                 "Compositor is missing unstable zxdg_shell_v6 support");

    xdg_surface = zxdg_shell_v6_get_xdg_surface(shell, surface);

    zxdg_surface_v6_add_listener(xdg_surface, &xdg_surface_listener, this);

    xdg_toplevel = zxdg_surface_v6_get_toplevel(xdg_surface);

    zxdg_toplevel_v6_add_listener(xdg_toplevel, &xdg_toplevel_listener, this);

    wl_surface_commit(surface);

    return 0;
  }

  void update_window_title(const std::string& title) {
    zxdg_toplevel_v6_set_title(xdg_toplevel, title.c_str());
  }

  void registry_global(wl_registry *registry, uint32_t name,
                       const char *interface) {
    if (strcmp(interface, "wl_compositor") == 0) {
      compositor = (wl_compositor*)
          wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, "zxdg_shell_v6") == 0) {
      shell = (zxdg_shell_v6*)
          wl_registry_bind(registry, name, &zxdg_shell_v6_interface, 1);
      zxdg_shell_v6_add_listener(shell, &xdg_shell_listener, this);
    } else if (strcmp(interface, "wl_seat") == 0) {
      seat = (wl_seat*)
          wl_registry_bind(registry, name, &wl_seat_interface, 4);
      wl_seat_add_listener(seat, &seat_listener, this);
    } else if (strcmp(interface, "wl_output") == 0) {
      wl_output* _output = (wl_output*)
          wl_registry_bind(registry, name, &wl_output_interface, 2);
      wl_output_add_listener(_output, &output_listener, this);
    }
  }

  void fullscreen() {
    fullscreen(current_display()->output);
  }

  void fullscreen(wl_output *output) {
    zxdg_toplevel_v6_set_fullscreen(xdg_toplevel, output);
    wl_surface_commit(surface);
  }

  static void _xdg_surface_configure_cb(void *data, zxdg_surface_v6 *surface,
                                        uint32_t serial) {
    zxdg_surface_v6_ack_configure(surface, serial);
  }

  static void _xdg_toplevel_configure_cb(void *data, zxdg_toplevel_v6 *toplevel,
                                         int32_t width, int32_t height,
                                         struct wl_array *states) {
    WindowWaylandXDG *self = reinterpret_cast<WindowWaylandXDG *>(data);
    self->configure(width, height);
  }

  const zxdg_surface_v6_listener xdg_surface_listener = {
    _xdg_surface_configure_cb,
  };

  const zxdg_toplevel_v6_listener xdg_toplevel_listener = {
    _xdg_toplevel_configure_cb,
    _xdg_toplevel_close_cb,
  };

  const zxdg_shell_v6_listener xdg_shell_listener = {
    _xdg_shell_ping_cb,
  };

  static void _xdg_shell_ping_cb(void *data, zxdg_shell_v6 *shell,
                                 uint32_t serial) {
    zxdg_shell_v6_pong(shell, serial);
  }

  // unused callbacks

  static void _xdg_toplevel_close_cb(void *data, zxdg_toplevel_v6 *toplevel) {}
};
}  // namespace vik
