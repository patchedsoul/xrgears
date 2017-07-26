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

#include <wayland-client.h>

#include <vulkan/vulkan.h>

#include "vksWindow.hpp"
#include "vksApplication.hpp"

#include <linux/input.h>

// TODO: hack for +/- keys
//#define KEY_KPADD KEY_KPPLUS
//#define KEY_KPSUB KEY_KPMINUS

class ApplicationWayland : public Application {

  wl_display *display = nullptr;
  wl_registry *registry = nullptr;
  wl_compositor *compositor = nullptr;
  wl_shell *shell = nullptr;
  wl_seat *seat = nullptr;
  wl_pointer *pointer = nullptr;
  wl_keyboard *keyboard = nullptr;
  wl_surface *surface = nullptr;
  wl_shell_surface *shell_surface = nullptr;

public:
  explicit ApplicationWayland(bool enableValidation) : Application(enableValidation) {
    display = wl_display_connect(NULL);
    if (!display)
    {
      std::cout << "Could not connect to Wayland display!\n";
      fflush(stdout);
      exit(1);
    }

    registry = wl_display_get_registry(display);
    if (!registry)
    {
      std::cout << "Could not get Wayland registry!\n";
      fflush(stdout);
      exit(1);
    }

    static const struct wl_registry_listener registry_listener =
    { registryGlobalCb, registryGlobalRemoveCb };
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);
    if (!compositor || !shell || !seat)
    {
      std::cout << "Could not bind Wayland protocols!\n";
      fflush(stdout);
      exit(1);
    }
  }

  ~ApplicationWayland() {
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

  void renderLoop() {
    while (!quit)
    {
      auto tStart = std::chrono::high_resolution_clock::now();
      if (viewUpdated)
      {
        viewUpdated = false;
        viewChanged();
      }

      while (wl_display_prepare_read(display) != 0)
        wl_display_dispatch_pending(display);
      wl_display_flush(display);
      wl_display_read_events(display);
      wl_display_dispatch_pending(display);

      render();
      frameCounter++;
      auto tEnd = std::chrono::high_resolution_clock::now();
      auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
      frameTimer = tDiff / 1000.0f;
      camera.update(frameTimer);
      if (camera.moving())
      {
        viewUpdated = true;
      }
      // Convert to clamped timer value
      if (!paused)
      {
        timer += timerSpeed * frameTimer;
        if (timer > 1.0)
        {
          timer -= 1.0f;
        }
      }
      fpsTimer += (float)tDiff;
      if (fpsTimer > 1000.0f)
      {
        if (!enableTextOverlay)
        {
          std::string windowTitle = getWindowTitle();
          wl_shell_surface_set_title(shell_surface, windowTitle.c_str());
        }
        lastFPS = frameCounter;
        updateTextOverlay();
        fpsTimer = 0.0f;
        frameCounter = 0;
      }
    }
  }

  void initSwapChain() {
    VkResult err = VK_SUCCESS;

    VkWaylandSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.display = display;
    surfaceCreateInfo.surface = surface;
    err = vkCreateWaylandSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &swapChain.surface);

    if (err != VK_SUCCESS)
      vks::tools::exitFatal("Could not create surface!", "Fatal error");
    else
      swapChain.initSurfaceCommon();
  }

  // wayland
  static void registryGlobalCb(void *data,
                               wl_registry *registry, uint32_t name, const char *interface,
                               uint32_t version)
  {
    ApplicationWayland *self = reinterpret_cast<ApplicationWayland *>(data);
    self->registryGlobal(registry, name, interface, version);
  }

  static void seatCapabilitiesCb(void *data, wl_seat *seat,
                                 uint32_t caps)
  {
    ApplicationWayland *self = reinterpret_cast<ApplicationWayland *>(data);
    self->seatCapabilities(seat, caps);
  }

  static void pointerEnterCb(void *data,
                             wl_pointer *pointer, uint32_t serial, wl_surface *surface,
                             wl_fixed_t sx, wl_fixed_t sy)
  {
  }

  static void pointerLeaveCb(void *data,
                             wl_pointer *pointer, uint32_t serial, wl_surface *surface)
  {
  }

  static void pointerMotionCb(void *data,
                              wl_pointer *pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
  {
    ApplicationWayland *self = reinterpret_cast<ApplicationWayland *>(data);
    self->pointerMotion(pointer, time, sx, sy);
  }
  void pointerMotion(wl_pointer *pointer, uint32_t time,
                     wl_fixed_t sx, wl_fixed_t sy)
  {
    double x = wl_fixed_to_double(sx);
    double y = wl_fixed_to_double(sy);

    double dx = mousePos.x - x;
    double dy = mousePos.y - y;

    if (mouseButtons.left)
    {
      rotation.x += dy * 1.25f * rotationSpeed;
      rotation.y -= dx * 1.25f * rotationSpeed;
      camera.rotate(glm::vec3(
                      dy * camera.rotationSpeed,
                      -dx * camera.rotationSpeed,
                      0.0f));
      viewUpdated = true;
    }
    if (mouseButtons.right)
    {
      zoom += dy * .005f * zoomSpeed;
      camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f * zoomSpeed));
      viewUpdated = true;
    }
    if (mouseButtons.middle)
    {
      cameraPos.x -= dx * 0.01f;
      cameraPos.y -= dy * 0.01f;
      camera.translate(glm::vec3(-dx * 0.01f, -dy * 0.01f, 0.0f));
      viewUpdated = true;
    }
    mousePos = glm::vec2(x, y);
  }

  static void pointerButtonCb(void *data,
                              wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button,
                              uint32_t state)
  {
    ApplicationWayland *self = reinterpret_cast<ApplicationWayland *>(data);
    self->pointerButton(pointer, serial, time, button, state);
  }

  void pointerButton(struct wl_pointer *pointer,
                     uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
  {
    switch (button)
    {
      case BTN_LEFT:
        mouseButtons.left = !!state;
        break;
      case BTN_MIDDLE:
        mouseButtons.middle = !!state;
        break;
      case BTN_RIGHT:
        mouseButtons.right = !!state;
        break;
      default:
        break;
    }
  }

  static void pointerAxisCb(void *data,
                            wl_pointer *pointer, uint32_t time, uint32_t axis,
                            wl_fixed_t value)
  {
    ApplicationWayland *self = reinterpret_cast<ApplicationWayland *>(data);
    self->pointerAxis(pointer, time, axis, value);
  }

  void pointerAxis(wl_pointer *pointer, uint32_t time,
                   uint32_t axis, wl_fixed_t value)
  {
    double d = wl_fixed_to_double(value);
    switch (axis)
    {
      case REL_X:
        zoom += d * 0.005f * zoomSpeed;
        camera.translate(glm::vec3(0.0f, 0.0f, d * 0.005f * zoomSpeed));
        viewUpdated = true;
        break;
      default:
        break;
    }
  }

  static void keyboardKeymapCb(void *data,
                               struct wl_keyboard *keyboard, uint32_t format, int fd, uint32_t size)
  {
  }

  static void keyboardEnterCb(void *data,
                              struct wl_keyboard *keyboard, uint32_t serial,
                              struct wl_surface *surface, struct wl_array *keys)
  {
  }

  static void keyboardLeaveCb(void *data,
                              struct wl_keyboard *keyboard, uint32_t serial,
                              struct wl_surface *surface)
  {
  }

  static void keyboardKeyCb(void *data,
                            struct wl_keyboard *keyboard, uint32_t serial, uint32_t time,
                            uint32_t key, uint32_t state)
  {
    ApplicationWayland *self = reinterpret_cast<ApplicationWayland *>(data);
    self->keyboardKey(keyboard, serial, time, key, state);
  }

  void keyboardKey(struct wl_keyboard *keyboard,
                   uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
  {
    switch (key)
    {
      case KEY_W:
        camera.keys.up = !!state;
        break;
      case KEY_S:
        camera.keys.down = !!state;
        break;
      case KEY_A:
        camera.keys.left = !!state;
        break;
      case KEY_D:
        camera.keys.right = !!state;
        break;
      case KEY_P:
        if (state)
          paused = !paused;
        break;
      case KEY_F1:
        if (state && enableTextOverlay)
          textOverlay->visible = !textOverlay->visible;
        break;
      case KEY_ESC:
        quit = true;
        break;
    }

    if (state)
      keyPressed(key);
  }

  static void keyboardModifiersCb(void *data,
                                  struct wl_keyboard *keyboard, uint32_t serial, uint32_t mods_depressed,
                                  uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
  {
  }

  void seatCapabilities(wl_seat *seat, uint32_t caps)
  {
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !pointer)
    {
      pointer = wl_seat_get_pointer(seat);
      static const struct wl_pointer_listener pointer_listener =
      { pointerEnterCb, pointerLeaveCb, pointerMotionCb, pointerButtonCb,
            pointerAxisCb, };
      wl_pointer_add_listener(pointer, &pointer_listener, this);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && pointer)
    {
      wl_pointer_destroy(pointer);
      pointer = nullptr;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard)
    {
      keyboard = wl_seat_get_keyboard(seat);
      static const struct wl_keyboard_listener keyboard_listener =
      { keyboardKeymapCb, keyboardEnterCb, keyboardLeaveCb, keyboardKeyCb,
            keyboardModifiersCb, };
      wl_keyboard_add_listener(keyboard, &keyboard_listener, this);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && keyboard)
    {
      wl_keyboard_destroy(keyboard);
      keyboard = nullptr;
    }
  }

  void registryGlobal(wl_registry *registry, uint32_t name,
                      const char *interface, uint32_t version)
  {
    if (strcmp(interface, "wl_compositor") == 0)
    {
      compositor = (wl_compositor *) wl_registry_bind(registry, name,
                                                      &wl_compositor_interface, 3);
    }
    else if (strcmp(interface, "wl_shell") == 0)
    {
      shell = (wl_shell *) wl_registry_bind(registry, name,
                                            &wl_shell_interface, 1);
    }
    else if (strcmp(interface, "wl_seat") == 0)
    {
      seat = (wl_seat *) wl_registry_bind(registry, name, &wl_seat_interface,
                                          1);

      static const struct wl_seat_listener seat_listener =
      { seatCapabilitiesCb, };
      wl_seat_add_listener(seat, &seat_listener, this);
    }
  }

  static void registryGlobalRemoveCb(void *data,
                                     struct wl_registry *registry, uint32_t name)
  {
  }



  static void PingCb(void *data, struct wl_shell_surface *shell_surface,
                     uint32_t serial)
  {
    wl_shell_surface_pong(shell_surface, serial);
  }

  static void ConfigureCb(void *data, struct wl_shell_surface *shell_surface,
                          uint32_t edges, int32_t width, int32_t height)
  {
  }

  static void PopupDoneCb(void *data, struct wl_shell_surface *shell_surface)
  {
  }

  wl_shell_surface *setupWaylandWindow()
  {
    surface = wl_compositor_create_surface(compositor);
    shell_surface = wl_shell_get_shell_surface(shell, surface);

    static const struct wl_shell_surface_listener shell_surface_listener =
    { PingCb, ConfigureCb, PopupDoneCb };

    wl_shell_surface_add_listener(shell_surface, &shell_surface_listener, this);
    wl_shell_surface_set_toplevel(shell_surface);
    std::string windowTitle = getWindowTitle();
    wl_shell_surface_set_title(shell_surface, windowTitle.c_str());
    return shell_surface;
  }

  const char* requiredExtensionName() {
    return VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
  }

};
