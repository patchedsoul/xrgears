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

  struct zxdg_shell_v6 *shell;
  struct zxdg_surface_v6 *xdg_surface;
  struct zxdg_toplevel_v6 *xdg_toplevel;
  bool wait_for_configure;
  SwapChainVK swap_chain;

public:
  WindowWaylandXDG(Settings *s) : WindowWayland(s) {
    name = "wayland-xdg";
    seat = NULL;
    keyboard = NULL;
    shell = NULL;
  }

  ~WindowWaylandXDG() {}

  // Return -1 on failure.
  int init(uint32_t width, uint32_t height) {
    display = wl_display_connect(NULL);
    if (!display)
      return -1;

    struct wl_registry *registry = wl_display_get_registry(display);
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

  void flush() {
    while (wl_display_prepare_read(display) != 0)
      wl_display_dispatch_pending(display);
    if (wl_display_flush(display) < 0 && errno != EAGAIN) {
      wl_display_cancel_read(display);
      return;
    }

    pollfd fds[] = {{ wl_display_get_fd(display), POLLIN },};
    if (poll(fds, 1, 0) > 0) {
      wl_display_read_events(display);
      wl_display_dispatch_pending(display);
    } else {
      wl_display_cancel_read(display);
    }
  }

  void iterate(VkQueue queue, VkSemaphore semaphore) {
    flush();
    update_cb();
    swap_chain.render(queue, semaphore);
  }

  void init_swap_chain(uint32_t width, uint32_t height) {

    create_surface(swap_chain.instance, &swap_chain.surface);

    swap_chain.choose_surface_format();

    swap_chain.recreate(width, height);

    //recreate_frame_buffers_cb();
  }

  SwapChain* get_swap_chain() {
    return (SwapChain*) &swap_chain;
  }

  void update_window_title(const std::string& title) {
    zxdg_toplevel_v6_set_title(xdg_toplevel, title.c_str());
  }

  static void _xdg_surface_configure_cb(void *data, zxdg_surface_v6 *surface,
                                        uint32_t serial) {
    WindowWaylandXDG *self = reinterpret_cast<WindowWaylandXDG *>(data);
    zxdg_surface_v6_ack_configure(surface, serial);
    if (self->wait_for_configure)
      // redraw
      self->wait_for_configure = false;
  }

  void seat_capabilities(wl_seat *seat, uint32_t caps) {
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard) {
      keyboard = wl_seat_get_keyboard(seat);
      wl_keyboard_add_listener(keyboard, &keyboard_listener, this);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard) {
      wl_keyboard_destroy(keyboard);
      keyboard = NULL;
    }
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
      wl_output* the_output = (wl_output*)
          wl_registry_bind(registry, name, &wl_output_interface, 2);
      static const wl_output_listener _output_listener = {
        _output_geometry_cb,
        _output_mode_cb,
        _output_done_cb,
        _output_scale_cb
      };
      wl_output_add_listener(the_output, &_output_listener, this);
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

  // callback wrappers
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

  const wl_registry_listener registry_listener = {
    _registry_global_cb,
    _registry_global_remove_cb
  };

  const struct wl_seat_listener seat_listener = {
    _seat_capabilities_cb,
  };

  const struct wl_keyboard_listener keyboard_listener = {
    .keymap = _keyboard_keymap_cb,
    .enter = _keyboard_enter_cb,
    .leave = _keyboard_leave_cb,
    .key = _keyboard_key_cb,
    .modifiers = _keyboard_modifiers_cb,
    .repeat_info = _keyboard_repeat_cb,
  };

  static void _xdg_shell_ping_cb(void *data, zxdg_shell_v6 *shell,
                                 uint32_t serial) {
    zxdg_shell_v6_pong(shell, serial);
  }

  // unused callbacks
  static void _keyboard_repeat_cb(void *data, wl_keyboard *wl_keyboard,
                                  int32_t rate, int32_t delay) {}
  static void _keyboard_enter_cb(void *data, wl_keyboard *wl_keyboard,
                                 uint32_t serial, wl_surface *surface,
                                 wl_array *keys) {}
  static void _keyboard_leave_cb(void *data, wl_keyboard *wl_keyboard,
                                 uint32_t serial, wl_surface *surface) {}
  static void _xdg_toplevel_configure_cb(void *data, zxdg_toplevel_v6 *toplevel,
                                         int32_t width, int32_t height,
                                         struct wl_array *states) {}
  static void _xdg_toplevel_close_cb(void *data, zxdg_toplevel_v6 *toplevel) {}

};
}
