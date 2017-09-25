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
#include "../vks/vksSwapChain.hpp"

namespace vik {
class WindowWaylandShell : public WindowWayland {

  wl_registry *registry = nullptr;
  wl_shell *shell = nullptr;
  wl_pointer *pointer = nullptr;
  wl_shell_surface *shell_surface = nullptr;

public:
  explicit WindowWaylandShell() {
    name = "wayland-shell";
    display = wl_display_connect(NULL);
    vik_log_f_if(!display, "Could not connect to Wayland display!");

    registry = wl_display_get_registry(display);
    vik_log_f_if(!registry, "Could not get Wayland registry!");

    static const struct wl_registry_listener registry_listener =
    { registryGlobalCb, registryGlobalRemoveCb };
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

  int init(vik::Renderer *r) {
    surface = wl_compositor_create_surface(compositor);
    shell_surface = wl_shell_get_shell_surface(shell, surface);

    static const struct wl_shell_surface_listener shell_surface_listener =
    { PingCb, ConfigureCb, PopupDoneCb };

    wl_shell_surface_add_listener(shell_surface, &shell_surface_listener, this);
    //wl_shell_surface_set_toplevel(shell_surface);

    vik_log_d("setting hmd refresh to %d", hmd_refresh);
    vik_log_d("setting hmd output to %p", hmd_output);

    if (r->settings->fullscreen)
      wl_shell_surface_set_fullscreen(shell_surface,
                                      WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                      hmd_refresh,
                                      hmd_output);
    return 0;
  }

  void iterate(vik::Renderer *r) {
    while (wl_display_prepare_read(display) != 0)
      wl_display_dispatch_pending(display);
    wl_display_flush(display);
    wl_display_read_events(display);
    wl_display_dispatch_pending(display);
  }

  void init_swap_chain(vik::Renderer *r) {
    r->swap_chain = new vks::SwapChain();
    vks::SwapChain *sc = (vks::SwapChain*) r->swap_chain;
    sc->set_context(r->instance, r->physical_device, r->device);

    VkResult err = create_surface(r->instance, &sc->surface);
    vik_log_f_if(err != VK_SUCCESS, "Could not create surface!");

    sc->select_queue_and_format();
  }

  void update_window_title(const std::string& title) {
    wl_shell_surface_set_title(shell_surface, title.c_str());
  }

  static void outputModeCb(void *data, struct wl_output *wl_output,
                           unsigned int flags, int w, int h, int refresh) {
    vik_log_i("outputModeCb: %dx%d@%d", w, h, refresh);

    //    if (w == 2560 && h == 1440) {
    if (w == 1920 && h == 1200) {
      WindowWaylandShell *self = reinterpret_cast<WindowWaylandShell *>(data);
      vik_log_d("setting wl_output to %p", wl_output);
      self->hmd_output = wl_output;
      self->hmd_refresh = refresh;
    } else {
      vik_log_d("ignoring wl_output %p", wl_output);
    }
  }

  void seatCapabilities(wl_seat *seat, uint32_t caps) {
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !pointer) {
      pointer = wl_seat_get_pointer(seat);
      static const struct wl_pointer_listener pointer_listener =
      { pointerEnterCb, pointerLeaveCb, pointerMotionCb, pointerButtonCb,
            pointerAxisCb, };
      wl_pointer_add_listener(pointer, &pointer_listener, this);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && pointer) {
      wl_pointer_destroy(pointer);
      pointer = nullptr;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard) {
      keyboard = wl_seat_get_keyboard(seat);
      static const struct wl_keyboard_listener keyboard_listener =
      { keyboardKeymapCb, keyboardEnterCb, keyboardLeaveCb, keyboardKeyCb,
            keyboardModifiersCb, };
      wl_keyboard_add_listener(keyboard, &keyboard_listener, this);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard) {
      wl_keyboard_destroy(keyboard);
      keyboard = nullptr;
    }
  }

  void registryGlobal(wl_registry *registry, uint32_t name,
                      const char *interface, uint32_t version) {
    if (strcmp(interface, "wl_compositor") == 0) {
      compositor = (wl_compositor *) wl_registry_bind(registry, name, &wl_compositor_interface, 3);
    } else if (strcmp(interface, "wl_shell") == 0) {
      shell = (wl_shell *) wl_registry_bind(registry, name, &wl_shell_interface, 1);
    } else if (strcmp(interface, "wl_seat") == 0) {
      seat = (wl_seat *) wl_registry_bind(registry, name, &wl_seat_interface, 1);
      static const struct wl_seat_listener seat_listener =
      { seatCapabilitiesCb, };
      wl_seat_add_listener(seat, &seat_listener, this);
    } else if (strcmp(interface, "wl_output") == 0) {
      wl_output* the_output = (wl_output*) wl_registry_bind(registry, name, &wl_output_interface, 2);

      static const struct wl_output_listener _output_listener = {
        outputGeometryCb,
            outputModeCb,
            outputDoneCb,
            outputScaleCb
      };

      wl_output_add_listener(the_output, &_output_listener, this);
      
    }
  }

  // debug callbacks
  static void ConfigureCb(void *data, struct wl_shell_surface *shell_surface,
                          uint32_t edges, int32_t width, int32_t height) {
    vik_log_d("configure: %dx%d", width, height);
  }

  static void
  outputDoneCb(void *data, struct wl_output *output) {
    vik_log_d("output done %p", output);
  }

  static void
  outputScaleCb(void *data, struct wl_output *output, int scale) {
    vik_log_d("output scale: %d", scale);
  }

  static void outputGeometryCb(void *data, struct wl_output *wl_output, int x,
                               int y, int w, int h, int subpixel, const char *make, const char *model,
                               int transform) {
    //VikWindowWayland *self = reinterpret_cast<VikWindowWayland *>(data);
    vik_log_i("%s: %s [%d, %d] %dx%d", make, model, x, y, w, h);
  }

  // callback wrappers

  static void PingCb(void *data, struct wl_shell_surface *shell_surface,
                     uint32_t serial) {
    wl_shell_surface_pong(shell_surface, serial);
  }

  static void registryGlobalCb(void *data, wl_registry *registry, uint32_t name,
                               const char *interface, uint32_t version) {
    WindowWaylandShell *self = reinterpret_cast<WindowWaylandShell *>(data);
    self->registryGlobal(registry, name, interface, version);
  }

  static void keyboardKeyCb(void *data,
                            struct wl_keyboard *keyboard, uint32_t serial, uint32_t time,
                            uint32_t key, uint32_t state) {
    WindowWaylandShell *self = reinterpret_cast<WindowWaylandShell *>(data);
    self->keyboard_key_cb(wayland_to_vik_key(key), state);
  }

  static void seatCapabilitiesCb(void *data, wl_seat *seat, uint32_t caps) {
    WindowWaylandShell *self = reinterpret_cast<WindowWaylandShell *>(data);
    self->seatCapabilities(seat, caps);
  }

  static void pointerMotionCb(void *data, wl_pointer *pointer, uint32_t time,
                              wl_fixed_t x, wl_fixed_t y) {
    WindowWaylandShell *self = reinterpret_cast<WindowWaylandShell *>(data);
    self->pointer_motion_cb(wl_fixed_to_double(x),
                            wl_fixed_to_double(y));
  }

  static void pointerButtonCb(void *data,
                              wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button,
                              uint32_t state) {
    WindowWaylandShell *self = reinterpret_cast<WindowWaylandShell *>(data);
    self->pointer_button_cb(wayland_to_vik_button(button), state);
  }

  static void pointerAxisCb(void *data,
                            wl_pointer *pointer, uint32_t time, uint32_t axis,
                            wl_fixed_t value) {
    WindowWaylandShell *self = reinterpret_cast<WindowWaylandShell *>(data);
    double d = wl_fixed_to_double(value);
    self->pointer_axis_cb(wayland_to_vik_axis(axis), d);
  }

  // Unused callbacks
  static void registryGlobalRemoveCb(void *data, struct wl_registry *registry,
                                     uint32_t name) {}

  static void PopupDoneCb(void *data, struct wl_shell_surface *shell_surface) {}

  static void keyboardModifiersCb(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, uint32_t mods_depressed,
                                  uint32_t mods_latched, uint32_t mods_locked,
                                  uint32_t group) {}

  static void keyboardKeymapCb(void *data,
                               struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
  {}

  static void keyboardEnterCb(void *data,
                              struct wl_keyboard *keyboard, uint32_t serial,
                              struct wl_surface *surface, struct wl_array *keys)
  {}

  static void keyboardLeaveCb(void *data,
                              struct wl_keyboard *keyboard, uint32_t serial,
                              struct wl_surface *surface)
  {}

  static void pointerEnterCb(void *data, wl_pointer *pointer, uint32_t serial,
                             wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy)
  {}

  static void pointerLeaveCb(void *data, wl_pointer *pointer, uint32_t serial,
                             wl_surface *surface)
  {}


};
}
