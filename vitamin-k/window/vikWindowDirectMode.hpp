/*
 * vitamin-k
 *
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <xcb/xcb.h>
#include <xcb/randr.h>
#include <X11/extensions/Xrandr.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xlib_xrandr.h>

#include <string>
#include <vector>
#include <utility>

#include "vikWindow.hpp"

#include "../render/vikSwapChainVK.hpp"

namespace vik {
class WindowDirectMode : public Window {
  xcb_connection_t *connection;
  xcb_screen_t *screen;

  std::map<uint32_t, xcb_randr_mode_info_t> modes;

  SwapChainVK swap_chain;

  struct VikDisplay {
    std::string name;
    xcb_randr_output_t output;
    xcb_randr_mode_info_t primary_mode;
  };

  std::vector<VikDisplay> displays;

 public:
  explicit WindowDirectMode(Settings *s) : Window(s) {
    name = "direct";
  }

  ~WindowDirectMode() {
    xcb_disconnect(connection);
  }

  int init() {
    if (!connect())
      return -1;

    xcb_screen_iterator_t iter =
        xcb_setup_roots_iterator(xcb_get_setup(connection));

    screen = iter.data;

    get_randr_outputs();

    if (settings->list_screens_and_exit) {
      int display_i = 0;
      for (VikDisplay d : displays) {
        vik_log_i("%d: %s %dx%d@%.2f",
                  display_i,
                  d.name.c_str(),
                  d.primary_mode.width, d.primary_mode.height,
                  (double) d.primary_mode.dot_clock /
                  (d.primary_mode.htotal * d.primary_mode.vtotal));
        display_i++;
      }
      exit(0);
    }

    vik_log_w("Requested display %d", settings->display);

    if (settings->display > displays.size() - 1) {
      vik_log_w("Requested display %d, but only %d displays are available.",
                settings->display, displays.size());

      settings->display = 0;
      VikDisplay *d = current_display();
      vik_log_w("Selecting '%s' instead.", d->name.c_str());
    }

    VikDisplay *d = current_display();
    settings->size.first = d->primary_mode.width;
    settings->size.second = d->primary_mode.height;
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
    vik_log_i("Will use display: %s %dx%d@%.2f",
              d->name.c_str(),
              d->primary_mode.width, d->primary_mode.height,
              (double) d->primary_mode.dot_clock /
              (d->primary_mode.htotal * d->primary_mode.vtotal));

    VkDisplayKHR display = get_xlib_randr_output(d->output);

    aquire_xlib_display(display);

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

  int connect() {
    connection = xcb_connect(nullptr, nullptr);
    return !xcb_connection_has_error(connection);
  }

  void aquire_xlib_display(VkDisplayKHR display) {
    Display *dpy = XOpenDisplay(nullptr);
    vik_log_f_if(dpy == nullptr, "Could not open X display.");

    PFN_vkAcquireXlibDisplayEXT fun =
      (PFN_vkAcquireXlibDisplayEXT)
        vkGetInstanceProcAddr(swap_chain.instance, "vkAcquireXlibDisplayEXT");
    vik_log_f_if(fun == nullptr,
                 "Could not Get Device Proc Addr vkAcquireXlibDisplayEXT.");

    VkResult res = fun(swap_chain.physical_device, dpy, display);
    vik_log_f_if(res != VK_SUCCESS,
                 "Could not acquire Xlib display %p: %s",
                 display,
                 Log::result_string(res).c_str());
  }

  VkDisplayKHR get_xlib_randr_output(RROutput output) {
    Display *dpy = XOpenDisplay(nullptr);
    vik_log_f_if(dpy == nullptr, "Could not open X display.");

    PFN_vkGetRandROutputDisplayEXT fun =
      (PFN_vkGetRandROutputDisplayEXT)
        vkGetInstanceProcAddr(swap_chain.instance,
                              "vkGetRandROutputDisplayEXT");

    VkDisplayKHR display;
    VkResult res = fun(swap_chain.physical_device, dpy, output, &display);
    vik_log_f_if(res != VK_SUCCESS,
                 "Could not get RandR output display: %s",
                 Log::result_string(res).c_str());

    return display;
  }

  void
  enumerate_modes(xcb_randr_get_screen_resources_reply_t *resources_reply) {
    xcb_randr_mode_info_t *mode_infos =
      xcb_randr_get_screen_resources_modes (resources_reply);

    int n = xcb_randr_get_screen_resources_modes_length (resources_reply);
    for (int i = 0; i < n; i++)
      modes.insert(std::pair<uint32_t, xcb_randr_mode_info_t>(mode_infos[i].id,
                                                              mode_infos[i]));
  }

  void get_randr_outputs() {
    xcb_randr_query_version_cookie_t version_cookie =
      xcb_randr_query_version(connection,
                              XCB_RANDR_MAJOR_VERSION,
                              XCB_RANDR_MINOR_VERSION);
    xcb_randr_query_version_reply_t *version_reply =
      xcb_randr_query_version_reply(connection, version_cookie, NULL);

    vik_log_f_if(!version_reply, "Could not get RandR version.");

    vik_log_d("RandR version %d.%d",
              version_reply->major_version,
              version_reply->minor_version);

    if (version_reply->major_version < 1 || version_reply->minor_version < 6)
      vik_log_f("RandR version below 1.6.");

    xcb_generic_error_t *error = nullptr;
    xcb_intern_atom_cookie_t non_desktop_cookie =
      xcb_intern_atom(connection, 1, strlen("non-desktop"), "non-desktop");
    xcb_intern_atom_reply_t *non_desktop_reply =
      xcb_intern_atom_reply(connection, non_desktop_cookie, &error);

    if (error != nullptr)
      vik_log_f("xcb_intern_atom_reply returned error %d", error->error_code);

    vik_log_f_if(non_desktop_reply == nullptr, "non-desktop reply nullptr");

    vik_log_f_if(non_desktop_reply->atom == XCB_NONE,
                 "No output has non-desktop property");

    xcb_randr_get_screen_resources_cookie_t resources_cookie =
      xcb_randr_get_screen_resources(connection, screen->root);
    xcb_randr_get_screen_resources_reply_t *resources_reply =
      xcb_randr_get_screen_resources_reply(connection,
                                           resources_cookie,
                                           nullptr);
    xcb_randr_output_t *xcb_outputs =
      xcb_randr_get_screen_resources_outputs(resources_reply);

    enumerate_modes (resources_reply);

    int count = xcb_randr_get_screen_resources_outputs_length(resources_reply);
    if (count < 1)
      vik_log_f("failed to retrieve randr outputs");

    for (int i = 0; i < count; i++) {
      xcb_randr_get_output_info_cookie_t output_cookie =
          xcb_randr_get_output_info(connection,
                                    xcb_outputs[i], XCB_CURRENT_TIME);
      xcb_randr_get_output_info_reply_t *output_reply =
          xcb_randr_get_output_info_reply(connection, output_cookie, nullptr);

      uint8_t *name = xcb_randr_get_output_info_name(output_reply);
      int name_len = xcb_randr_get_output_info_name_length(output_reply);

      char* name_str = (char*) malloc(name_len + 1);
      memcpy(name_str, name, name_len);
      name_str[name_len] = '\0';

      /* Find the first output that has the non-desktop property set */
      xcb_randr_get_output_property_cookie_t prop_cookie;
      prop_cookie = xcb_randr_get_output_property (connection,
                                                   xcb_outputs[i],
                                                   non_desktop_reply->atom,
                                                   XCB_ATOM_NONE, 0, 4, 0, 0);
      xcb_randr_get_output_property_reply_t *prop_reply = nullptr;
      prop_reply = xcb_randr_get_output_property_reply (connection,
                                                        prop_cookie,
                                                        &error);
      if (error != nullptr)
        vik_log_f("xcb_randr_get_output_property_reply returned error %d",
                  error->error_code);

      vik_log_f_if(prop_reply == nullptr, "property reply == nullptr");

      vik_log_f_if (prop_reply->type != XCB_ATOM_INTEGER ||
                    prop_reply->num_items != 1 ||
                    prop_reply->format != 32,
                    "Invalid non-desktop reply");

      uint8_t non_desktop = *xcb_randr_get_output_property_data (prop_reply);
      if (non_desktop == 1) {
        xcb_randr_mode_t * output_modes =
          xcb_randr_get_output_info_modes (output_reply);

        VikDisplay d = {
          .name = std::string(name_str),
          .output = xcb_outputs[i],
          .primary_mode = modes.at(output_modes[0])
        };

        displays.push_back(d);
      }

      free(name_str);
    }
  }

 public:
  const std::vector<const char*> required_extensions() {
    return {
      VK_KHR_DISPLAY_EXTENSION_NAME,
      VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
      VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME
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
