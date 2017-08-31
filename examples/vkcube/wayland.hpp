#pragma once

#include <string.h>
#include <stdio.h>

#include <xdg-shell-unstable-v6-client-protocol.h>
#include <wayland-client.h>
#include <linux/input.h>

#include "display.hpp"

#include "application.hpp"

#include "silo.h"


class VikDisplayModeWayland : public VikDisplayMode {

  struct wl_display *display;
  struct wl_compositor *compositor;
  struct zxdg_shell_v6 *shell;
  struct wl_keyboard *keyboard;
  struct wl_seat *seat;
  struct wl_surface *surface;
  struct zxdg_surface_v6 *xdg_surface;
  struct zxdg_toplevel_v6 *xdg_toplevel;
  bool wait_for_configure;

  int hmd_refresh = 0;
  wl_output *hmd_output = nullptr;

public:
  VikDisplayModeWayland() {
    name = "wayland-xdg";
    seat = NULL;
    keyboard = NULL;
    shell = NULL;
  }

  ~VikDisplayModeWayland() {}

  static void
  handle_xdg_surface_configure(void *data, struct zxdg_surface_v6 *surface,
                               uint32_t serial)
  {
    VikDisplayModeWayland *self = reinterpret_cast<VikDisplayModeWayland *>(data);

    zxdg_surface_v6_ack_configure(surface, serial);

    if (self->wait_for_configure) {
      // redraw
      self->wait_for_configure = false;
    }
  }

  const struct zxdg_surface_v6_listener xdg_surface_listener = {
    handle_xdg_surface_configure,
  };

  static void
  handle_xdg_toplevel_configure(void *data, struct zxdg_toplevel_v6 *toplevel,
                                int32_t width, int32_t height,
                                struct wl_array *states)
  {
  }

  static void
  handle_xdg_toplevel_close(void *data, struct zxdg_toplevel_v6 *toplevel)
  {
  }

  const struct zxdg_toplevel_v6_listener xdg_toplevel_listener = {
    handle_xdg_toplevel_configure,
    handle_xdg_toplevel_close,
  };

  static void
  handle_xdg_shell_ping(void *data, struct zxdg_shell_v6 *shell, uint32_t serial)
  {
    zxdg_shell_v6_pong(shell, serial);
  }

  const struct zxdg_shell_v6_listener xdg_shell_listener = {
    handle_xdg_shell_ping,
  };

  static void
  handle_wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
                            uint32_t format, int32_t fd, uint32_t size)
  {
  }

  static void
  handle_wl_keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
                           uint32_t serial, struct wl_surface *surface,
                           struct wl_array *keys)
  {
  }

  static void
  handle_wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
                           uint32_t serial, struct wl_surface *surface)
  {
  }

  static void
  handle_wl_keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
                         uint32_t serial, uint32_t time, uint32_t key,
                         uint32_t state)
  {
    if (key == KEY_ESC && state == WL_KEYBOARD_KEY_STATE_PRESSED)
      exit(0);
  }

  static void
  handle_wl_keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
                               uint32_t serial, uint32_t mods_depressed,
                               uint32_t mods_latched, uint32_t mods_locked,
                               uint32_t group)
  {
  }

  static void
  handle_wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
                                 int32_t rate, int32_t delay)
  {
  }

  const struct wl_keyboard_listener wl_keyboard_listener = {
    .keymap = handle_wl_keyboard_keymap,
    .enter = handle_wl_keyboard_enter,
    .leave = handle_wl_keyboard_leave,
    .key = handle_wl_keyboard_key,
    .modifiers = handle_wl_keyboard_modifiers,
    .repeat_info = handle_wl_keyboard_repeat_info,
  };

  static void
  handle_wl_seat_capabilities(void *data, struct wl_seat *wl_seat,
                              uint32_t capabilities)
  {
    VikDisplayModeWayland *self = reinterpret_cast<VikDisplayModeWayland *>(data);

    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && (!self->keyboard)) {
      self->keyboard = wl_seat_get_keyboard(wl_seat);
      wl_keyboard_add_listener(self->keyboard, &self->wl_keyboard_listener, self);
    } else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && self->keyboard) {
      wl_keyboard_destroy(self->keyboard);
      self->keyboard = NULL;
    }
  }

  const struct wl_seat_listener wl_seat_listener = {
    handle_wl_seat_capabilities,
  };

  static void
  registry_handle_global(void *data, struct wl_registry *registry,
                         uint32_t name, const char *interface, uint32_t version)
  {
    VikDisplayModeWayland *self = reinterpret_cast<VikDisplayModeWayland *>(data);

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
      static const struct wl_output_listener _output_listener = {
        outputGeometryCb,
            outputModeCb,
            outputDoneCb,
            outputScaleCb
      };
      wl_output_add_listener(the_output, &_output_listener, self);
    }
  }

  static void
  registry_handle_global_remove(void *data, struct wl_registry *registry,
                                uint32_t name)
  {
  }

  const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
  };

  static void outputGeometryCb(void *data, struct wl_output *wl_output, int x,
                               int y, int w, int h, int subpixel, const char *make, const char *model,
                               int transform) {
    //VikWindowWayland *self = reinterpret_cast<VikWindowWayland *>(data);
    printf("%s: %s [%d, %d] %dx%d\n", make, model, x, y, w, h);
  }

  static void outputModeCb(void *data, struct wl_output *wl_output,
                           unsigned int flags, int w, int h, int refresh) {
    printf("outputModeCb: %dx%d@%d\n", w, h, refresh);

    if (w == 2560 && h == 1440) {
    //if (w == 1920 && h == 1200) {
      VikDisplayModeWayland *self = reinterpret_cast<VikDisplayModeWayland *>(data);
      printf("setting wl_output to %p\n", wl_output);
      self->hmd_output = wl_output;
      self->hmd_refresh = refresh;
      zxdg_toplevel_v6_set_fullscreen(self->xdg_toplevel, self->hmd_output);
      wl_surface_commit(self->surface);
    } else {
      printf("ignoring wl_output %p\n", wl_output);
    }
  }

  static void
  outputDoneCb(void *data, struct wl_output *output) {
    printf("output done %p\n", output);
  }

  static void
  outputScaleCb(void *data, struct wl_output *output, int scale) {
    printf("output scale: %d\n", scale);
  }

  // Return -1 on failure.
  int
  init(CubeApplication* app, CubeRenderer *vc)
  {
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

    if (!shell)
      log_fatal("Compositor is missing unstable zxdg_shell_v6 protocol support");

    xdg_surface = zxdg_shell_v6_get_xdg_surface(shell, surface);

    zxdg_surface_v6_add_listener(xdg_surface, &xdg_surface_listener, this);

    xdg_toplevel = zxdg_surface_v6_get_toplevel(xdg_surface);

    //zxdg_surface_v6_get_popup()

    //zxdg_positioner_v6_set_size();

    printf("the hmd output is %p\n", hmd_output);

    zxdg_toplevel_v6_add_listener(xdg_toplevel, &xdg_toplevel_listener, this);

    zxdg_toplevel_v6_set_title(xdg_toplevel, "vkcube");

    //zxdg_surface_v6_set_window_geometry(xdg_surface, 2560, 0, 1920, 1200);

    //zxdg_toplevel_v6_set_maximized(xdg_toplevel);

    wait_for_configure = true;
    wl_surface_commit(surface);

    vc->init_vk(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);

    PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR get_wayland_presentation_support =
        (PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR)
        vkGetInstanceProcAddr(vc->instance, "vkGetPhysicalDeviceWaylandPresentationSupportKHR");
    PFN_vkCreateWaylandSurfaceKHR create_wayland_surface =
        (PFN_vkCreateWaylandSurfaceKHR)
        vkGetInstanceProcAddr(vc->instance, "vkCreateWaylandSurfaceKHR");

    if (!get_wayland_presentation_support(vc->physical_device, 0,
                                          display)) {
      log_fatal("Vulkan not supported on given Wayland surface");
    }

    VkWaylandSurfaceCreateInfoKHR waylandSurfaceInfo = {
      .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
      .display = display,
      .surface = surface,
    };

    create_wayland_surface(vc->instance, &waylandSurfaceInfo, NULL, &vc->surface);

    vc->image_format = vc->choose_surface_format();

    vc->init_vk_objects(&app->model);

    vc->create_swapchain();

    return 0;
  }

  void
  main_loop(CubeApplication* app, CubeRenderer *vc)
  {
    VkResult result = VK_SUCCESS;
    struct pollfd fds[] = {
    { wl_display_get_fd(display), POLLIN },
  };
    while (1) {
      uint32_t index;

      while (wl_display_prepare_read(display) != 0)
        wl_display_dispatch_pending(display);
      if (wl_display_flush(display) < 0 && errno != EAGAIN) {
        wl_display_cancel_read(display);
        return;
      }
      if (poll(fds, 1, 0) > 0) {
        wl_display_read_events(display);
        wl_display_dispatch_pending(display);
      } else {
        wl_display_cancel_read(display);
      }

      result = vkAcquireNextImageKHR(vc->device, vc->swap_chain, 60,
                                     vc->semaphore, VK_NULL_HANDLE, &index);
      if (result != VK_SUCCESS)
        return;

      //vc->model.render(vc, &vc->buffers[index]);
      app->model.render(vc, &vc->buffers[index]);

      VkSwapchainKHR swapChains[] = { vc->swap_chain, };
      uint32_t indices[] = { index, };

      VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = swapChains,
        .pImageIndices = indices,
        .pResults = &result,
      };

      vkQueuePresentKHR(vc->queue, &presentInfo);
      if (result != VK_SUCCESS)
        return;
    }
  }
};
