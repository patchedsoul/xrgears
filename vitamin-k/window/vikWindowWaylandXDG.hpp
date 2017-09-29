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

  static void handle_xdg_surface_configure(void *data, zxdg_surface_v6 *surface,
                                           uint32_t serial) {
    WindowWaylandXDG *self = reinterpret_cast<WindowWaylandXDG *>(data);
    zxdg_surface_v6_ack_configure(surface, serial);
    if (self->wait_for_configure)
      // redraw
      self->wait_for_configure = false;
  }

  static void handle_wl_seat_capabilities(void *data, wl_seat *wl_seat,
                                          uint32_t capabilities) {
    WindowWaylandXDG *self = reinterpret_cast<WindowWaylandXDG *>(data);

    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && (!self->keyboard)) {
      self->keyboard = wl_seat_get_keyboard(wl_seat);
      wl_keyboard_add_listener(self->keyboard, &self->wl_keyboard_listener, self);
    } else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && self->keyboard) {
      wl_keyboard_destroy(self->keyboard);
      self->keyboard = NULL;
    }
  }

  static void registry_handle_global(void *data, wl_registry *registry,
                                     uint32_t name, const char *interface, uint32_t version) {
    WindowWaylandXDG *self = reinterpret_cast<WindowWaylandXDG *>(data);

    if (strcmp(interface, "wl_compositor") == 0) {
      self->compositor = ( wl_compositor *) wl_registry_bind(registry, name,
                                                             &wl_compositor_interface, 1);
    } else if (strcmp(interface, "zxdg_shell_v6") == 0) {
      self->shell = (zxdg_shell_v6 *)  wl_registry_bind(registry, name, &zxdg_shell_v6_interface, 1);
      zxdg_shell_v6_add_listener(self->shell, &self->xdg_shell_listener, self);
    } else if (strcmp(interface, "wl_seat") == 0) {
      self->seat = (wl_seat *) wl_registry_bind(registry, name, &wl_seat_interface, 1);
      wl_seat_add_listener(self->seat, &self->wl_seat_listener, self);
    } else if (strcmp(interface, "wl_output") == 0) {
      wl_output* the_output = (wl_output*) wl_registry_bind(registry, name, &wl_output_interface, 2);
      static const wl_output_listener _output_listener = {
        outputGeometryCb,
        outputModeCb,
        outputDoneCb,
        outputScaleCb
      };
      wl_output_add_listener(the_output, &_output_listener, self);
    }
  }

  static void outputModeCb(void *data, wl_output *wl_output,
                           unsigned int flags, int w, int h, int refresh) {
    vik_log_i("outputModeCb: %dx%d@%d", w, h, refresh);

    if (w == 2560 && h == 1440) {
      //if (w == 1920 && h == 1200) {
      WindowWaylandXDG *self = reinterpret_cast<WindowWaylandXDG *>(data);
      vik_log_d("setting wl_output to %p", wl_output);
      self->hmd_output = wl_output;
      self->hmd_refresh = refresh;
      zxdg_toplevel_v6_set_fullscreen(self->xdg_toplevel, self->hmd_output);
      wl_surface_commit(self->surface);
    } else {
      vik_log_d("ignoring wl_output %p", wl_output);
    }
  }

  // debug callbacks
  static void
  outputDoneCb(void *data, wl_output *output) {
    vik_log_d("output done %p", output);
  }

  static void
  outputScaleCb(void *data, wl_output *output, int scale) {
    vik_log_d("output scale: %d", scale);
  }

  static void outputGeometryCb(void *data, wl_output *wl_output, int x,
                               int y, int w, int h, int subpixel, const char *make, const char *model,
                               int transform) {
    //VikWindowWayland *self = reinterpret_cast<VikWindowWayland *>(data);
    vik_log_i("%s: %s [%d, %d] %dx%d", make, model, x, y, w, h);
  }

  // callback wrappers

  const zxdg_surface_v6_listener xdg_surface_listener = {
    handle_xdg_surface_configure,
  };

  const zxdg_toplevel_v6_listener xdg_toplevel_listener = {
    handle_xdg_toplevel_configure,
    handle_xdg_toplevel_close,
  };

  const zxdg_shell_v6_listener xdg_shell_listener = {
    handle_xdg_shell_ping,
  };

  const wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
  };

  const struct wl_seat_listener wl_seat_listener = {
    handle_wl_seat_capabilities,
  };

  const struct wl_keyboard_listener wl_keyboard_listener = {
    .keymap = handle_wl_keyboard_keymap,
    .enter = handle_wl_keyboard_enter,
    .leave = handle_wl_keyboard_leave,
    .key = _keyboard_key_cb,
    .modifiers = handle_wl_keyboard_modifiers,
    .repeat_info = handle_wl_keyboard_repeat_info,
  };

  static void
  handle_xdg_shell_ping(void *data, zxdg_shell_v6 *shell, uint32_t serial) {
    zxdg_shell_v6_pong(shell, serial);
  }

  // unused callbacks
  static void handle_wl_keyboard_modifiers(void *data, wl_keyboard *wl_keyboard,
                                           uint32_t serial, uint32_t mods_depressed,
                                           uint32_t mods_latched, uint32_t mods_locked,
                                           uint32_t group) {}
  static void handle_wl_keyboard_repeat_info(void *data, wl_keyboard *wl_keyboard,
                                             int32_t rate, int32_t delay) {}
  static void registry_handle_global_remove(void *data, wl_registry *registry,
                                            uint32_t name) {}
  static void handle_wl_keyboard_keymap(void *data, wl_keyboard *wl_keyboard,
                                        uint32_t format, int32_t fd, uint32_t size) {}
  static void handle_wl_keyboard_enter(void *data, wl_keyboard *wl_keyboard,
                                       uint32_t serial, wl_surface *surface,
                                       wl_array *keys) {}
  static void handle_wl_keyboard_leave(void *data, wl_keyboard *wl_keyboard,
                                       uint32_t serial, wl_surface *surface) {}
  static void handle_xdg_toplevel_configure(void *data, zxdg_toplevel_v6 *toplevel,
                                            int32_t width, int32_t height,
                                            struct wl_array *states) {}
  static void handle_xdg_toplevel_close(void *data, zxdg_toplevel_v6 *toplevel) {}

};
}
