/*
 * XRGears
 *
 * Copyright (C) 2016 Sascha Willems - www.saschawillems.de
 * Copyright (C) 2017 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 *
 * Based on Vulkan Examples written by Sascha Willems
 */

#pragma once

#include <vulkan/vulkan.h>

#include "vikWindowWayland.hpp"


namespace vik {
class WindowWaylandShell : public WindowWayland {

  wl_shell *shell = nullptr;
  wl_shell_surface *shell_surface = nullptr;

public:
  WindowWaylandShell(Settings *s) : WindowWayland(s) {
    name = "wayland-shell";
  }

  ~WindowWaylandShell() {
    wl_shell_surface_destroy(shell_surface);
    wl_shell_destroy(shell);
  }

  int init(uint32_t width, uint32_t height) {
    display = wl_display_connect(NULL);
    vik_log_f_if(!display, "Could not connect to Wayland display!");

    wl_registry *registry = wl_display_get_registry(display);
    vik_log_f_if(!registry, "Could not get Wayland registry!");

    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    vik_log_f_if(!compositor || !shell || !seat,
                 "Could not bind Wayland protocols!");
    wl_registry_destroy(registry);

    surface = wl_compositor_create_surface(compositor);
    shell_surface = wl_shell_get_shell_surface(shell, surface);

    wl_shell_surface_add_listener(shell_surface, &shell_surface_listener, this);
    //wl_shell_surface_set_toplevel(shell_surface);

    vik_log_d("setting hmd refresh to %d", hmd_refresh);
    vik_log_d("setting hmd output to %p", hmd_output);

    if (settings->fullscreen)
      wl_shell_surface_set_fullscreen(shell_surface,
                                      WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                      hmd_refresh,
                                      hmd_output);
    return 0;
  }

  void update_window_title(const std::string& title) {
    wl_shell_surface_set_title(shell_surface, title.c_str());
  }

  void output_mode(wl_output *wl_output, unsigned int flags,
                   int w, int h, int refresh) {
    vik_log_i("outputModeCb: %dx%d@%d", w, h, refresh);
    //    if (w == 2560 && h == 1440) {
    if (w == 1920 && h == 1200) {
      vik_log_d("setting wl_output to %p", wl_output);
      hmd_output = wl_output;
      hmd_refresh = refresh;
    } else {
      vik_log_d("ignoring wl_output %p", wl_output);
    }
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
    vik_log_d("configure: %dx%d", width, height);
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
}
