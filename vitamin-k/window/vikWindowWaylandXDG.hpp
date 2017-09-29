#pragma once

#include <string.h>
#include <stdio.h>

#include <poll.h>

#include <vulkan/vulkan.h>
#include "render/vikRendererVkc.hpp"
#include "../xdg-shell/xdg-shell-unstable-v6-client-protocol.h"
#include "system/vikLog.hpp"
#include "vikWindowWayland.hpp"

namespace vik {
class WindowWaylandXDG : public WindowWayland {

  struct zxdg_shell_v6 *shell = nullptr;
  struct zxdg_surface_v6 *xdg_surface = nullptr;
  struct zxdg_toplevel_v6 *xdg_toplevel = nullptr;
  bool wait_for_configure;
  SwapChainVkComplex swap_chain;

public:
  WindowWaylandXDG(Settings *s) : WindowWayland(s) {
    name = "wayland-xdg";
  }

  ~WindowWaylandXDG() {}

  // Return -1 on failure.
  int init(uint32_t width, uint32_t height) {
    display = wl_display_connect(NULL);
    if (!display)
      return -1;

    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, this);

    /* Round-trip to get globals */
    wl_display_roundtrip(display);

    /* We don't need this anymore */
    wl_registry_destroy(registry);

    surface = wl_compositor_create_surface(compositor);

    vik_log_f_if(!shell, "Compositor is missing unstable zxdg_shell_v6 protocol support");

    xdg_surface = zxdg_shell_v6_get_xdg_surface(shell, surface);

    zxdg_surface_v6_add_listener(xdg_surface, &xdg_surface_listener, this);

    xdg_toplevel = zxdg_surface_v6_get_toplevel(xdg_surface);

    //zxdg_surface_v6_get_popup()

    //zxdg_positioner_v6_set_size();

    vik_log_d("the hmd output is %p", hmd_output);

    zxdg_toplevel_v6_add_listener(xdg_toplevel, &xdg_toplevel_listener, this);

    update_window_title("vkcube");

    //zxdg_surface_v6_set_window_geometry(xdg_surface, 2560, 0, 1920, 1200);

    //zxdg_toplevel_v6_set_maximized(xdg_toplevel);

    wait_for_configure = true;
    wl_surface_commit(surface);

    return 0;
  }

  void update_window_title(const std::string& title) {
    zxdg_toplevel_v6_set_title(xdg_toplevel, title.c_str());
  }

  void registry_global(wl_registry *registry, uint32_t name,
                       const char *interface) {
    if (strcmp(interface, "wl_compositor") == 0) {
      compositor = ( wl_compositor*)
          wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "zxdg_shell_v6") == 0) {
      shell = (zxdg_shell_v6*)
          wl_registry_bind(registry, name, &zxdg_shell_v6_interface, 1);
      zxdg_shell_v6_add_listener(shell, &xdg_shell_listener, this);
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

  void output_mode(wl_output *wl_output, unsigned int flags,
                   int w, int h, int refresh) {
    vik_log_i("outputModeCb: %dx%d@%d", w, h, refresh);
    if (w == 2560 && h == 1440) {
      //if (w == 1920 && h == 1200) {
      vik_log_d("setting wl_output to %p", wl_output);
      hmd_output = wl_output;
      hmd_refresh = refresh;
      zxdg_toplevel_v6_set_fullscreen(xdg_toplevel, hmd_output);
      wl_surface_commit(surface);
    } else {
      vik_log_d("ignoring wl_output %p", wl_output);
    }
  }

  static void _xdg_surface_configure_cb(void *data, zxdg_surface_v6 *surface,
                                        uint32_t serial) {
    WindowWaylandXDG *self = reinterpret_cast<WindowWaylandXDG *>(data);
    zxdg_surface_v6_ack_configure(surface, serial);
    if (self->wait_for_configure)
      // redraw
      self->wait_for_configure = false;
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
  static void _xdg_toplevel_configure_cb(void *data, zxdg_toplevel_v6 *toplevel,
                                         int32_t width, int32_t height,
                                         struct wl_array *states) {}
  static void _xdg_toplevel_close_cb(void *data, zxdg_toplevel_v6 *toplevel) {}

};
}
