/*
 * vitamin-k
 *
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <algorithm>
#include <utility>

#include "vikLog.hpp"
#include "../render/vikTools.hpp"

namespace vik {
class Settings {
 public:
  enum WindowType {
    AUTO = 0,
    KMS,
    XCB,
    WAYLAND_XDG,
    WAYLAND_SHELL,
    KHR_DISPLAY,
    INVALID
  };

  int gpu = -1;
  int hmd = -1;

  int display = -1;
  int mode = -1;

  VkFormat color_format = VK_FORMAT_B8G8R8A8_UNORM;
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

  enum WindowType window_type = AUTO;

  bool validation = false;
  bool fullscreen = false;

  bool list_gpus_and_exit = false;
  bool list_screens_and_exit = false;
  bool list_hmds_and_exit = false;
  bool list_formats_and_exit = false;
  bool list_present_modes_and_exit = false;

  bool mouse_navigation = false;

  enum DistortionType {
    DISTORTION_TYPE_NONE = 0,
    DISTORTION_TYPE_PANOTOOLS,
    DISTORTION_TYPE_VIVE,
    DISTORTION_TYPE_INVALID
  };

  static DistortionType distortion_type_from_string(const char *s) {
    if (streq(s, "none"))
      return DISTORTION_TYPE_NONE;
    else if (streq(s, "panotools"))
      return DISTORTION_TYPE_PANOTOOLS;
    else if (streq(s, "vive"))
      return DISTORTION_TYPE_VIVE;
    else
      return DISTORTION_TYPE_INVALID;
  }

  enum DistortionType distortion_type = DISTORTION_TYPE_PANOTOOLS;

  bool enable_text_overlay = true;

  std::pair<uint32_t, uint32_t> size = {1280, 720};

  std::string help_string() {
    return
        "A XR demo for Vulkan and OpenHMD\n"
        "\n"
        "Options:\n"
        "  -s, --size WxH           Size of the output window (default: 1280x720)\n"
        "  -f, --fullscreen         Run fullscreen. Optinally specify display and mode.\n"
        "  -d, --display D          Display to fullscreen on. (default: 0)\n"
        "  -m, --mode M             Mode for fullscreen (wayland-shell only) (default: 0)\n"
        "  -w, --window WS          Window system to use (default: auto)\n"
        "                           [xcb, wayland, wayland-shell, kms]\n"
        "  -g, --gpu GPU            GPU to use (default: 0)\n"
        "      --hmd HMD            HMD to use (default: 0)\n"
        "      --format F           Color format to use (default: VK_FORMAT_B8G8R8A8_UNORM)\n"
        "      --presentmode M      Present mode to use (default: VK_PRESENT_MODE_FIFO_KHR)\n"
        "\n"
        "      --list-gpus          List available GPUs\n"
        "      --list-displays      List available displays\n"
        "      --list-hmds          List available HMDs\n"
        "      --list-formats       List available color formats\n"
        "      --list-presentmodes  List available present modes\n"
        "\n"
        "      --disable-overlay    Disable text overlay\n"
        "      --mouse-navigation   Use mouse instead of HMD for camera control.\n"
        "      --distortion         HMD lens distortion (default: panotools)\n"
        "                           [none, panotools, vive]\n"
        "  -v, --validation         Run Vulkan validation\n"
        "  -h, --help               Show this help\n";
  }

  std::pair<uint32_t, uint32_t> parse_size(std::string const& str) {
      std::pair<uint32_t, uint32_t> size;
      auto const dimensions = tools::split(str, 'x');

      if (dimensions.size() != 2
          || !is_number(dimensions[0])
          || !is_number(dimensions[1]))
        vik_log_f("Size must be 2 integers separated with x.");

      size.first = tools::from_string<uint32_t>(dimensions[0]);

      /*
       * Parse the second element (height). If there is none, use the value
       * of the first element for the second (width = height)
       */
      if (dimensions.size() > 1)
          size.second = tools::from_string<uint32_t>(dimensions[1]);
      else
          size.second = size.first;

      return size;
  }

  bool parse_args(int argc, char *argv[]) {
    /* Setting '+' in the optstring is the same as setting POSIXLY_CORRECT in
     * the enviroment. It tells getopt to stop parsing argv when it encounters
     * the first non-option argument; it also prevents getopt from permuting
     * argv during parsing.
     *
     * The initial ':' in the optstring makes getopt return ':' when an option
     * is missing a required argument.
     */

    int option_index = -1;
    static const char *optstring = "hs:w:vfg:d:m:";


    struct option long_options[] = {
      {"window", 1, 0, 0},
      {"size", 1, 0, 0},
      {"help", 0, 0, 0},
      {"validation", 0, 0, 0},
      {"fullscreen", 0, 0, 0},
      {"display", 1, 0, 0},
      {"mode", 1, 0, 0},
      {"gpu", 1, 0, 0},
      {"hmd", 1, 0, 0},
      {"format", 1, 0, 0},
      {"presentmode", 1, 0, 0},
      {"list-gpus", 0, 0, 0},
      {"list-displays", 0, 0, 0},
      {"list-hmds", 0, 0, 0},
      {"list-formats", 0, 0, 0},
      {"list-presentmodes", 0, 0, 0},
      {"disable-overlay", 0, 0, 0},
      {"mouse-navigation", 0, 0, 0},
      {"distortion", 1, 0, 0},
      {0, 0, 0, 0}
    };

    std::string optname;

    int opt;
    while ((opt = getopt_long(argc, argv, optstring,
                              long_options, &option_index)) != -1) {
      if (opt == '?' || opt == ':')
        return false;

      if (option_index != -1)
        optname = long_options[option_index].name;

      if (opt == 'h' || optname == "help") {
        printf("%s\n", help_string().c_str());
        exit(0);
      } else if (opt == 'v' || optname == "validation") {
        validation = true;
      } else if (optname == "list-gpus") {
        list_gpus_and_exit = true;
      } else if (optname == "list-displays") {
        list_screens_and_exit = true;
      } else if (optname == "list-hmds") {
        list_hmds_and_exit = true;
      } else if (optname == "list-formats") {
        list_formats_and_exit = true;
      } else if (optname == "list-presentmodes") {
        list_present_modes_and_exit = true;
      } else if (optname == "disable-overlay") {
        enable_text_overlay = false;
      } else if (opt == 's' || optname == "size") {
        size = parse_size(optarg);
      } else if (optname == "presentmode") {
        present_mode = Log::string_to_present_mode(optarg);
      } else if (optname == "format") {
        color_format = Log::string_to_color_format(optarg);
      } else if (opt == 'f' || optname == "fullscreen") {
        fullscreen = true;
      } else if (opt == 'd' || optname == "display") {
        display = parse_id(optarg);
        fullscreen = true;
      } else if (opt == 'm' || optname == "mode") {
        mode = parse_id(optarg);
        fullscreen = true;
      } else if (optname == "hmd") {
        hmd = parse_id(optarg);
      } else if (opt == 'g' || optname == "gpu") {
        gpu = parse_id(optarg);
      } else if (opt == 'w' || optname == "window") {
        window_type = window_type_from_string(optarg);
        if (window_type == INVALID)
          vik_log_f("option -w given bad display mode");
      } else if (optname == "mouse-navigation") {
        mouse_navigation = true;
      } else if (optname == "distortion") {
        distortion_type = distortion_type_from_string(optarg);
        if (distortion_type == DISTORTION_TYPE_INVALID)
          vik_log_f("option --distortion_type given bad type.");
      } else {
        vik_log_f("Unknown option %s", optname.c_str());
      }
    }

    if (optind != argc)
      vik_log_w("trailing args");

    return true;
  }

  bool is_number(const std::string& str) {
    auto is_not_digit = [](char c) { return !std::isdigit(c); };
    return !str.empty() &&
        std::find_if(str.begin(), str.end(), is_not_digit) == str.end();
  }

  int parse_id(std::string const& str) {
    if (!is_number(str)) {
      vik_log_e("%s is not a valid number", str.c_str());
      return 0;
    }
    return std::stoi(str, nullptr);
  }

  static inline bool streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
  }

  static WindowType window_type_from_string(const char *s) {
    if (streq(s, "auto"))
      return AUTO;
    else if (streq(s, "kms"))
      return KMS;
    else if (streq(s, "xcb"))
      return XCB;
    else if (streq(s, "wayland") || streq(s, "wayland-xdg"))
      return WAYLAND_XDG;
    else if (streq(s, "wayland-shell"))
      return WAYLAND_SHELL;
    else if (streq(s, "khr-display"))
      return KHR_DISPLAY;
    else
      return INVALID;
  }

};
}  // namespace vik
