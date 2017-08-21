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

#include <linux/input.h>
#include <wayland-client.h>
#include <vulkan/vulkan.h>

#include <string>

#include "vksWindow.hpp"
#include "vksApplication.hpp"

class VikWindowWayland : public VikWindow {
  wl_display *display = nullptr;
  wl_registry *registry = nullptr;
  wl_compositor *compositor = nullptr;
  wl_shell *shell = nullptr;
  wl_seat *seat = nullptr;
  wl_pointer *pointer = nullptr;
  wl_keyboard *keyboard = nullptr;
  wl_surface *surface = nullptr;
  wl_shell_surface *shell_surface = nullptr;
  wl_output *hmd_output = nullptr;
  int hmd_refresh = 0;
  
  Application* app;

 public:
  explicit VikWindowWayland() {
    display = wl_display_connect(NULL);
    if (!display) {
      std::cout << "Could not connect to Wayland display!\n";
      fflush(stdout);
      exit(1);
    }

    registry = wl_display_get_registry(display);
    if (!registry) {
      std::cout << "Could not get Wayland registry!\n";
      fflush(stdout);
      exit(1);
    }

    static const struct wl_registry_listener registry_listener = { registryGlobalCb, registryGlobalRemoveCb };
    wl_registry_add_listener(registry, &registry_listener, this);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);
    if (!compositor || !shell || !seat) {
      std::cout << "Could not bind Wayland protocols!\n";
      fflush(stdout);
      exit(1);
    }
  }

  ~VikWindowWayland() {
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

  void renderLoop(Application *app) {
    while (!app->quit) {
      auto tStart = std::chrono::high_resolution_clock::now();
      if (app->viewUpdated) {
        app->viewUpdated = false;
        app->viewChanged();
      }

      while (wl_display_prepare_read(display) != 0)
        wl_display_dispatch_pending(display);
      wl_display_flush(display);
      wl_display_read_events(display);
      wl_display_dispatch_pending(display);

      app->render();
      app->frameCounter++;
      auto tEnd = std::chrono::high_resolution_clock::now();
      auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
      app->frameTimer = tDiff / 1000.0f;
      app->camera.update(app->frameTimer);
      if (app->camera.moving())
        app->viewUpdated = true;
      // Convert to clamped timer value
      if (!app->paused) {
        app->timer += app->timerSpeed * app->frameTimer;
        if (app->timer > 1.0)
          app->timer -= 1.0f;
      }
      app->fpsTimer += (float)tDiff;
      if (app->fpsTimer > 1000.0f) {
        if (!app->enableTextOverlay) {
          std::string windowTitle = app->getWindowTitle();
          wl_shell_surface_set_title(shell_surface, windowTitle.c_str());
        }
        app->lastFPS = app->frameCounter;
        app->updateTextOverlay();
        app->fpsTimer = 0.0f;
        app->frameCounter = 0;
      }
    }
  }

  void initSwapChain(const VkInstance &instance, VulkanSwapChain* swapChain) {
    VkResult err = VK_SUCCESS;

    VkWaylandSurfaceCreateInfoKHR surfaceCreateInfo = {};
    surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    surfaceCreateInfo.display = display;
    surfaceCreateInfo.surface = surface;
    err = vkCreateWaylandSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &swapChain->surface);

    if (err != VK_SUCCESS)
      vks::tools::exitFatal("Could not create surface!", "Fatal error");
    else
      swapChain->initSurfaceCommon();
  }

  static void registryGlobalCb(void *data, wl_registry *registry, uint32_t name,
                               const char *interface, uint32_t version) {
    VikWindowWayland *self = reinterpret_cast<VikWindowWayland *>(data);
    self->registryGlobal(registry, name, interface, version);
  }


  static void outputGeometryCb(void *data, struct wl_output *wl_output, int x,
      int y, int w, int h, int subpixel, const char *make, const char *model,
      int transform) {
    //VikWindowWayland *self = reinterpret_cast<VikWindowWayland *>(data);
        printf("%s: %s [%d, %d] %dx%d\n", make, model, x, y, w, h);
  }

    static void outputModeCb(void *data, struct wl_output *wl_output, 
          unsigned int flags, int w, int h, int refresh) {
        printf("outputModeCb: %dx%d@%d\n", w, h, refresh);
        
        //if (w == 2560 && h == 1440) {
        if (w == 1920 && h == 1200) {
          VikWindowWayland *self = reinterpret_cast<VikWindowWayland *>(data);
          printf("setting wl_output to %p\n", wl_output);
          self->hmd_output = wl_output;
          self->hmd_refresh = refresh;
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

  static void seatCapabilitiesCb(void *data, wl_seat *seat, uint32_t caps) {
    VikWindowWayland *self = reinterpret_cast<VikWindowWayland *>(data);
    self->seatCapabilities(seat, caps);
  }

  static void pointerEnterCb(void *data, wl_pointer *pointer, uint32_t serial,
                             wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy)
  {}

  static void pointerLeaveCb(void *data, wl_pointer *pointer, uint32_t serial,
                             wl_surface *surface)
  {}

  static void pointerMotionCb(void *data, wl_pointer *pointer, uint32_t time,
                              wl_fixed_t sx, wl_fixed_t sy) {
    VikWindowWayland *self = reinterpret_cast<VikWindowWayland *>(data);
    self->pointerMotion(pointer, time, sx, sy);
  }

  void pointerMotion(wl_pointer *pointer, uint32_t time,
                     wl_fixed_t sx, wl_fixed_t sy) {
    double x = wl_fixed_to_double(sx);
    double y = wl_fixed_to_double(sy);

    double dx = app->mousePos.x - x;
    double dy = app->mousePos.y - y;

    if (app->mouseButtons.left) {
      app->rotation.x += dy * 1.25f * app->rotationSpeed;
      app->rotation.y -= dx * 1.25f * app->rotationSpeed;
      app->camera.rotate(glm::vec3(
                      dy * app->camera.rotationSpeed,
                      -dx * app->camera.rotationSpeed,
                      0.0f));
      app->viewUpdated = true;
    }

    if (app->mouseButtons.right) {
      app->zoom += dy * .005f * app->zoomSpeed;
      app->camera.translate(glm::vec3(-0.0f, 0.0f, dy * .005f * app->zoomSpeed));
      app->viewUpdated = true;
    }

    if (app->mouseButtons.middle) {
      app->cameraPos.x -= dx * 0.01f;
      app->cameraPos.y -= dy * 0.01f;
      app->camera.translate(glm::vec3(-dx * 0.01f, -dy * 0.01f, 0.0f));
      app->viewUpdated = true;
    }
    app->mousePos = glm::vec2(x, y);
  }

  static void pointerButtonCb(void *data,
                              wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button,
                              uint32_t state) {
    VikWindowWayland *self = reinterpret_cast<VikWindowWayland *>(data);
    self->pointerButton(pointer, serial, time, button, state);
  }

  void pointerButton(struct wl_pointer *pointer,
                     uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    switch (button) {
      case BTN_LEFT:
        app->mouseButtons.left = !!state;
        break;
      case BTN_MIDDLE:
        app->mouseButtons.middle = !!state;
        break;
      case BTN_RIGHT:
        app->mouseButtons.right = !!state;
        break;
      default:
        break;
    }
  }

  static void pointerAxisCb(void *data,
                            wl_pointer *pointer, uint32_t time, uint32_t axis,
                            wl_fixed_t value) {
    VikWindowWayland *self = reinterpret_cast<VikWindowWayland *>(data);
    self->pointerAxis(pointer, time, axis, value);
  }

  void pointerAxis(wl_pointer *pointer, uint32_t time,
                   uint32_t axis, wl_fixed_t value) {
    double d = wl_fixed_to_double(value);
    switch (axis) {
      case REL_X:
        app->zoom += d * 0.005f * app->zoomSpeed;
        app->camera.translate(glm::vec3(0.0f, 0.0f, d * 0.005f * app->zoomSpeed));
        app->viewUpdated = true;
        break;
      default:
        break;
    }
  }

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

  static void keyboardKeyCb(void *data,
                            struct wl_keyboard *keyboard, uint32_t serial, uint32_t time,
                            uint32_t key, uint32_t state) {
    VikWindowWayland *self = reinterpret_cast<VikWindowWayland *>(data);
    self->keyboardKey(keyboard, serial, time, key, state);
  }

  void keyboardKey(struct wl_keyboard *keyboard,
                   uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
    switch (key) {
      case KEY_W:
        app->camera.keys.up = !!state;
        break;
      case KEY_S:
        app->camera.keys.down = !!state;
        break;
      case KEY_A:
        app->camera.keys.left = !!state;
        break;
      case KEY_D:
        app->camera.keys.right = !!state;
        break;
      case KEY_P:
        if (state)
          app->paused = !app->paused;
        break;
      case KEY_F1:
        if (state && app->enableTextOverlay)
          app->textOverlay->visible = !app->textOverlay->visible;
        break;
      case KEY_ESC:
        app->quit = true;
        break;
    }

    if (state)
      app->keyPressed(key);
  }

  static void keyboardModifiersCb(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, uint32_t mods_depressed,
                                  uint32_t mods_latched, uint32_t mods_locked,
                                  uint32_t group) {}

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

  static void registryGlobalRemoveCb(void *data, struct wl_registry *registry,
                                     uint32_t name) {}

  static void PingCb(void *data, struct wl_shell_surface *shell_surface,
                     uint32_t serial) {
    wl_shell_surface_pong(shell_surface, serial);
  }

  static void ConfigureCb(void *data, struct wl_shell_surface *shell_surface,
                          uint32_t edges, int32_t width, int32_t height) {
    printf("configure: %dx%d\n", width, height);
  }

  static void PopupDoneCb(void *data, struct wl_shell_surface *shell_surface) {}

  void setupWindow(Application * app) {
    this->app = app;
    surface = wl_compositor_create_surface(compositor);
    shell_surface = wl_shell_get_shell_surface(shell, surface);

    static const struct wl_shell_surface_listener shell_surface_listener =
    { PingCb, ConfigureCb, PopupDoneCb };

    wl_shell_surface_add_listener(shell_surface, &shell_surface_listener, this);
    //wl_shell_surface_set_toplevel(shell_surface);
    
    printf("setting hmd refresh to %d\n", hmd_refresh);
    printf("setting hmd output to %p\n", hmd_output);

    wl_shell_surface_set_fullscreen(shell_surface,
                                    WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT,
                                    hmd_refresh,
                                    hmd_output);

    std::string windowTitle = app->getWindowTitle();
    wl_shell_surface_set_title(shell_surface, windowTitle.c_str());
  }

  const char* requiredExtensionName() {
    return VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
  }
};
