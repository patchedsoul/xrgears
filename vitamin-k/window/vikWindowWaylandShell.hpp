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
#include "render/vikSwapChainVKComplex.hpp"

namespace vik {
class WindowWaylandShell : public WindowWayland {

  wl_registry *registry = nullptr;
  wl_shell *shell = nullptr;
  wl_pointer *pointer = nullptr;
  wl_shell_surface *shell_surface = nullptr;

  SwapChainVkComplex swap_chain;

public:
  WindowWaylandShell(Settings *s) : WindowWayland(s) {
    name = "wayland-shell";
    display = wl_display_connect(NULL);
    vik_log_f_if(!display, "Could not connect to Wayland display!");

    registry = wl_display_get_registry(display);
    vik_log_f_if(!registry, "Could not get Wayland registry!");

    static const wl_registry_listener registry_listener =
    { _registry_global_cb, _registry_global_remove_cb };
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);
    vik_log_f_if(!compositor || !shell || !seat, "Could not bind Wayland protocols!");
  }

  ~WindowWaylandShell() {
    wl_shell_surface_destroy(shell_surface);
    wl_surface_destroy(surface);
    if (keyboard)
      wl_keyboard_destroy(keyboard);
    if (pointer)
      wl_pointer_destroy(pointer);
    wl_seat_destroy(seat);
    wl_shell_destroy(shell);
    wl_compositor_destroy(compositor);
    wl_registry_destroy(registry);
    wl_display_disconnect(display);
  }

  int init(uint32_t width, uint32_t height) {
    surface = wl_compositor_create_surface(compositor);
    shell_surface = wl_shell_get_shell_surface(shell, surface);

    static const struct wl_shell_surface_listener shell_surface_listener =
    { _ping_cb, _configure_cb, _popup_done_cb };

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

  void iterate(VkQueue queue, VkSemaphore semaphore) {
    while (wl_display_prepare_read(display) != 0)
      wl_display_dispatch_pending(display);
    wl_display_flush(display);
    wl_display_read_events(display);
    wl_display_dispatch_pending(display);
  }

  void init_swap_chain(uint32_t width, uint32_t height) {
    VkResult err = create_surface(swap_chain.instance, &swap_chain.surface);
    vik_log_f_if(err != VK_SUCCESS, "Could not create surface!");
    swap_chain.set_dimension_cb(dimension_cb);
    swap_chain.select_queue_and_format();
    swap_chain.set_settings(settings);
    swap_chain.create(width, height);
  }

  SwapChain* get_swap_chain() {
    return (SwapChain*) &swap_chain;
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

  void seat_capabilities(wl_seat *seat, uint32_t caps) {
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !pointer) {
      pointer = wl_seat_get_pointer(seat);
      static const wl_pointer_listener pointer_listener =
      { _pointer_enter_cb, _pointer_leave_cb, _pointer_motion_cb, _pointer_button_cb,
        _pointer_axis_cb, };
      wl_pointer_add_listener(pointer, &pointer_listener, this);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && pointer) {
      wl_pointer_destroy(pointer);
      pointer = nullptr;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard) {
      keyboard = wl_seat_get_keyboard(seat);
      static const wl_keyboard_listener keyboard_listener =
      { _keyboard_keymap_cb, _keyboard_enter_cb, _keyboard_leave_cb, _keyboard_key_cb,
        _keyboard_modifiers_cb, };
      wl_keyboard_add_listener(keyboard, &keyboard_listener, this);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard) {
      wl_keyboard_destroy(keyboard);
      keyboard = nullptr;
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
      static const wl_seat_listener seat_listener = { _seat_capabilities_cb, };
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

  // callback wrappers
  static void _ping_cb(void *data, wl_shell_surface *shell_surface,
                       uint32_t serial) {
    wl_shell_surface_pong(shell_surface, serial);
  }

  // debug callbacks
  static void _configure_cb(void *data, wl_shell_surface *shell_surface,
                            uint32_t edges, int32_t width, int32_t height) {
    vik_log_d("configure: %dx%d", width, height);
  }

  // Unused callbacks
  static void _popup_done_cb(void *data, wl_shell_surface *shell_surface) {}
  static void _pointer_enter_cb(void *data, wl_pointer *pointer,
                                uint32_t serial, wl_surface *surface,
                                wl_fixed_t sx, wl_fixed_t sy) {}
  static void _pointer_leave_cb(void *data, wl_pointer *pointer,
                                uint32_t serial, wl_surface *surface) {}

};
}
