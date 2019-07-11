/*
 * vitamin-k
 *
 * Copyright 2017-2018 Collabora Ltd.
 * Copyright 2019 Status Holdings Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * Authors: Drew DeVault <sir@cmpwn.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <wayland-client.h>
#include "wayland-protocols/drm-lease-unstable-v1.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_wayland.h>

#include <string>
#include <vector>
#include <utility>

#include "vikWindow.hpp"

#include "../render/vikSwapChainVK.hpp"

namespace vik {
class WindowDirectWayland : public Window {
  struct wl_display *wl_display = nullptr;
  struct zwp_drm_lease_manager_v1 *manager = nullptr;

  SwapChainVK swap_chain;

  struct VikDisplay {
    std::string name, description;
    struct zwp_drm_lease_connector_v1 *connector;
    struct zwp_drm_lease_v1 *lease;
  };

  std::vector<VikDisplay> displays;

  static void lease_connector_handle_connector_id(void *data,
      struct zwp_drm_lease_connector_v1 *zwp_drm_lease_connector_v1,
	  int32_t conn_id) {
    /* This space deliberately left blank */
  }

  static void lease_connector_handle_name(void *data,
      struct zwp_drm_lease_connector_v1 *zwp_drm_lease_connector_v1,
      const char *name) {
    VikDisplay *d = (VikDisplay *)data;
    d->name = std::string(name);
  }

  static void lease_connector_handle_description(void *data,
      struct zwp_drm_lease_connector_v1 *zwp_drm_lease_connector_v1,
      const char *desc) {
    VikDisplay *d = (VikDisplay *)data;
    d->description = std::string(desc);
  }

  static void lease_connector_handle_withdrawn(void *data,
      struct zwp_drm_lease_connector_v1 *zwp_drm_lease_connector_v1) {
    /* This space deliberately left blank */
  }

  const struct zwp_drm_lease_connector_v1_listener lease_connector_listener = {
    .name = lease_connector_handle_name,
    .description = lease_connector_handle_description,
	.connector_id = lease_connector_handle_connector_id,
    .withdrawn = lease_connector_handle_withdrawn,
  };

  static void lease_manager_handle_drm_fd(void *data,
      struct zwp_drm_lease_manager_v1 *lease_manager, int fd) {
    close(fd);
  }

  static void lease_manager_handle_connector(void *data,
      struct zwp_drm_lease_manager_v1 *lease_manager,
      struct zwp_drm_lease_connector_v1 *id) {
    WindowDirectWayland *win = (WindowDirectWayland *)data;
    VikDisplay d = {
      .connector = id,
    };
    win->displays.push_back(d);
    zwp_drm_lease_connector_v1_add_listener(id, &win->lease_connector_listener,
        &win->displays[win->displays.size() - 1]);
  }

  static void lease_manager_handle_finished(void *data,
      struct zwp_drm_lease_manager_v1 *lease_manager) {
    /* This space deliberately left blank */
  }

  const struct zwp_drm_lease_manager_v1_listener lease_manager_listener = {
    .drm_fd = lease_manager_handle_drm_fd,
    .connector = lease_manager_handle_connector,
    .finished = lease_manager_handle_finished,
  };

  static void registry_handle_global(
      void *data, struct wl_registry *wl_registry, uint32_t name,
      const char *interface, uint32_t version) {
    WindowDirectWayland *win = (WindowDirectWayland *)data;
    if (strcmp(interface, zwp_drm_lease_manager_v1_interface.name) == 0) {
      win->manager = (struct zwp_drm_lease_manager_v1 *)wl_registry_bind(
          wl_registry, name, &zwp_drm_lease_manager_v1_interface, 1);
    }
  }

  static void registry_handle_global_remove(
      void *data, struct wl_registry *wl_registry, uint32_t name) {
    // Who cares
  }

  const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
  };

 public:
  explicit WindowDirectWayland(Settings *s) : Window(s) {
    name = "direct-wayland";
  }

  ~WindowDirectWayland() {
    if (manager)
      zwp_drm_lease_manager_v1_destroy(manager);
    if (wl_display)
      wl_display_disconnect(wl_display);
  }

  int init() {
    wl_display = wl_display_connect(NULL);
    if (wl_display == NULL)
      return -1;

    struct wl_registry *wl_registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(wl_registry, &registry_listener, this);
    wl_display_roundtrip(wl_display);
    if (manager == nullptr) {
      vik_log_f("Wayland compositor does not support drm-lease-unstable-v1");
      return -1;
    }
    zwp_drm_lease_manager_v1_add_listener(
        manager, &lease_manager_listener, this);
    wl_display_roundtrip(wl_display);
    /* Again to get connector details */
    wl_display_roundtrip(wl_display);

    if (settings->list_screens_and_exit) {
      int display_i = 0;
      for (VikDisplay d : displays) {
        vik_log_i("%d: %s %s",
                  display_i,
                  d.name.c_str(),
                  d.description.c_str());
        display_i++;
      }
      exit(0);
    }

    if (settings->display > (int) displays.size() - 1) {
      vik_log_w("Requested display %d, but only %d displays are available.",
                settings->display, displays.size());

      settings->display = 0;
      VikDisplay *d = current_display();
      vik_log_w("Selecting '%s' instead.", d->name.c_str());
    }

    if (settings->display < 0) {
      settings->display = 0;
      VikDisplay *d = current_display();
      vik_log_w("Selecting '%s' first display.", d->name.c_str());
    }

    settings->size.first = 2160;
    settings->size.second = 1200;
    size_only_cb(settings->size.first, settings->size.second);

    return 0;
  }

  VikDisplay* current_display() {
    return &displays[settings->display];
  }

  void iterate() {
    render_frame_cb();
  }

  VkDisplayModeKHR get_primary_display_mode (VkDisplayKHR display) {
    uint32_t mode_count;
    VkResult res =
      vkGetDisplayModePropertiesKHR(swap_chain.physical_device, display,
                                   &mode_count, nullptr);
    vik_log_f_if(res != VK_SUCCESS,
                 "Could not vkGetDisplayModePropertiesKHR: %s",
                 Log::result_string(res).c_str());

    vik_log_d("Found %d modes", mode_count);

    VkDisplayModePropertiesKHR* mode_properties;
    mode_properties = new VkDisplayModePropertiesKHR[mode_count];
    res = vkGetDisplayModePropertiesKHR(swap_chain.physical_device, display,
                                       &mode_count, mode_properties);
    vik_log_f_if(res != VK_SUCCESS,
                 "Could not vkGetDisplayModePropertiesKHR: %s",
                 Log::result_string(res).c_str());

    VkDisplayModePropertiesKHR props = mode_properties[0];

    vik_log_d("found dispkay mode %d %d",
      props.parameters.visibleRegion.width,
      props.parameters.visibleRegion.height);

    delete[] mode_properties;

    return props.displayMode;
  }

  VkDisplayPlaneAlphaFlagBitsKHR
  choose_alpha_mode (VkDisplayPlaneAlphaFlagsKHR flags) {
    if (flags & VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR)
      return VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR;
    else if (flags & VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR)
      return VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR;
    else
      return VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR;
  }

  void init_swap_chain(uint32_t width, uint32_t height) {
    VikDisplay *d = current_display();

    vik_log_i("Will use display: %s", d->name.c_str());

    VkDisplayKHR display = acquire_wl_display(current_display());

    // Get plane properties
    uint32_t plane_property_count;
    VkResult res =
      vkGetPhysicalDeviceDisplayPlanePropertiesKHR(swap_chain.physical_device,
                                                  &plane_property_count,
                                                   nullptr);
    vik_log_f_if(res != VK_SUCCESS,
                 "Could not vkGetPhysicalDeviceDisplayPlanePropertiesKHR: %s",
                 Log::result_string(res).c_str());

    vik_log_i("Found %d plane properites.", plane_property_count);

    VkDisplayPlanePropertiesKHR* plane_properties =
      new VkDisplayPlanePropertiesKHR[plane_property_count];

    res =
      vkGetPhysicalDeviceDisplayPlanePropertiesKHR(swap_chain.physical_device,
                                                  &plane_property_count,
                                                   plane_properties);
    vik_log_f_if(res != VK_SUCCESS,
                 "Could not vkGetPhysicalDeviceDisplayPlanePropertiesKHR: %s",
                 Log::result_string(res).c_str());

    uint32_t plane_index = 0;

    VkDisplayModeKHR display_mode = get_primary_display_mode (display);

    VkDisplayPlaneCapabilitiesKHR plane_caps;
    vkGetDisplayPlaneCapabilitiesKHR(swap_chain.physical_device, display_mode,
                                     plane_index, &plane_caps);

    VkDisplaySurfaceCreateInfoKHR surface_info = {
      .sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR,
      .flags = 0,
      .displayMode = display_mode,
      .planeIndex = plane_index,
      .planeStackIndex = plane_properties[plane_index].currentStackIndex,
      .transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .globalAlpha = 1.0,
      .alphaMode = choose_alpha_mode(plane_caps.supportedAlpha),
      .imageExtent = {
        .width = width,
        .height = height
      }
    };

    VkResult result = vkCreateDisplayPlaneSurfaceKHR(swap_chain.instance,
                                                     &surface_info, nullptr,
                                                     &swap_chain.surface);
    vik_log_f_if(result !=VK_SUCCESS, "Failed to create surface!");

    delete[] plane_properties;

    swap_chain.set_settings(settings);
    swap_chain.select_surface_format();
    swap_chain.create(width, height);
  }

  SwapChain* get_swap_chain() {
    return (SwapChain*) &swap_chain;
  }

  VkDisplayKHR acquire_wl_display(VikDisplay *dpy) {
    VkWaylandLeaseConnectorEXT connectors[] = {
      {
        .pConnector = dpy->connector,
      }
    };

    PFN_vkAcquireWaylandDisplayEXT fun =
      (PFN_vkAcquireWaylandDisplayEXT)
        vkGetInstanceProcAddr(swap_chain.instance, "vkAcquireWaylandDisplayEXT");
    vik_log_f_if(fun == nullptr,
                 "Could not Get Device Proc Addr vkAcquireWaylandDisplayEXT.");

    VkResult res = fun(swap_chain.physical_device,
        wl_display, manager, 1, connectors);
    vik_log_f_if(res != VK_SUCCESS,
                 "Could not acquire Wayland display: %s",
                 Log::result_string(res).c_str());
    return connectors[0].pDisplay;
  }

 public:
  const std::vector<const char*> required_extensions() {
    return {
      VK_KHR_DISPLAY_EXTENSION_NAME,
      VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
      VK_EXT_ACQUIRE_WL_DISPLAY_EXTENSION_NAME,
    };
  }

  const std::vector<const char*> required_device_extensions() {
    return {};
  }

  VkBool32 check_support(VkPhysicalDevice physical_device) {
    return true;
  }

  void update_window_title(const std::string& title) {}

};
}  // namespace vik
