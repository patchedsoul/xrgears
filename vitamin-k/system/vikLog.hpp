/*
 * vitamin-k
 *
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <cstring>
#include <iostream>

#include "vulkan/vulkan.h"

#define LOG_TO_STD_ERR 0

#define vik_log(...) vik::Log::log(__FILE__, __LINE__, __VA_ARGS__)
#define vik_log_d(...) vik_log(vik::Log::DEBUG, __VA_ARGS__)
#define vik_log_i(...) vik_log(vik::Log::INFO, __VA_ARGS__)
#define vik_log_w(...) vik_log(vik::Log::WARNING, __VA_ARGS__)
#define vik_log_e(...) vik_log(vik::Log::ERROR, __VA_ARGS__)
#define vik_log_f(...) vik_log(vik::Log::FATAL, __VA_ARGS__)
#define vik_log_if(...) vik::Log::log_if(__FILE__, __LINE__, __VA_ARGS__)
#define vik_log_f_if(...) vik_log_if(vik::Log::FATAL, __VA_ARGS__)
#define vik_log_e_if(...) vik_log_if(vik::Log::ERROR, __VA_ARGS__)
#define vik_log_i_short(...) vik::Log::log_short(vik::Log::INFO, __VA_ARGS__)

// Macro to check and display Vulkan return results
#define vik_log_check(f) {\
  VkResult res = (f);\
  vik_log_f_if(res != VK_SUCCESS, "VkResult is %s", vik::Log::result_string(res).c_str());\
}

namespace vik {
class Log {
  static const bool use_color = true;

 public:
  enum type {
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR,
    FATAL
  };

  /** @brief Returns an result code as a string */
  static std::string result_string(VkResult code) {
    switch (code) {
  #define STR(r) case r: return #r
      STR(VK_SUCCESS);
      STR(VK_NOT_READY);
      STR(VK_TIMEOUT);
      STR(VK_EVENT_SET);
      STR(VK_EVENT_RESET);
      STR(VK_INCOMPLETE);
      STR(VK_ERROR_OUT_OF_HOST_MEMORY);
      STR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
      STR(VK_ERROR_INITIALIZATION_FAILED);
      STR(VK_ERROR_DEVICE_LOST);
      STR(VK_ERROR_MEMORY_MAP_FAILED);
      STR(VK_ERROR_LAYER_NOT_PRESENT);
      STR(VK_ERROR_EXTENSION_NOT_PRESENT);
      STR(VK_ERROR_FEATURE_NOT_PRESENT);
      STR(VK_ERROR_INCOMPATIBLE_DRIVER);
      STR(VK_ERROR_TOO_MANY_OBJECTS);
      STR(VK_ERROR_FORMAT_NOT_SUPPORTED);
      STR(VK_ERROR_SURFACE_LOST_KHR);
      STR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
      STR(VK_SUBOPTIMAL_KHR);
      STR(VK_ERROR_OUT_OF_DATE_KHR);
      STR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
      STR(VK_ERROR_VALIDATION_FAILED_EXT);
      STR(VK_ERROR_INVALID_SHADER_NV);
  #undef STR
      default:
        return "UNKNOWN RESULT";
    }
  }

  static std::string color_format_string(VkFormat code) {
    switch(code) {
#define STR(r) case r: return #r
    STR(VK_FORMAT_B8G8R8A8_UNORM);
    STR(VK_FORMAT_UNDEFINED);
    STR(VK_FORMAT_R8G8B8A8_SRGB);
    STR(VK_FORMAT_B8G8R8A8_SRGB);
    STR(VK_FORMAT_R8G8B8_SRGB);
    STR(VK_FORMAT_B8G8R8_SRGB);
    STR(VK_FORMAT_R5G6B5_UNORM_PACK16);
    STR(VK_FORMAT_B5G6R5_UNORM_PACK16);
#undef STR
      default:
        return "UNKNOWN FORMAT";
    }
  }

#define STR_TO_FORMAT(r) if (str.compare(#r) == 0) return r

  static VkFormat string_to_color_format(const std::string& str) {
    STR_TO_FORMAT(VK_FORMAT_B8G8R8A8_UNORM);
    STR_TO_FORMAT(VK_FORMAT_UNDEFINED);
    STR_TO_FORMAT(VK_FORMAT_R8G8B8A8_SRGB);
    STR_TO_FORMAT(VK_FORMAT_B8G8R8A8_SRGB);
    STR_TO_FORMAT(VK_FORMAT_R8G8B8_SRGB);
    STR_TO_FORMAT(VK_FORMAT_B8G8R8_SRGB);
    STR_TO_FORMAT(VK_FORMAT_R5G6B5_UNORM_PACK16);
    STR_TO_FORMAT(VK_FORMAT_B5G6R5_UNORM_PACK16);
    vik_log_w("Unknown format %s", str.c_str());
    return VK_FORMAT_UNDEFINED;
  }

  static std::string color_space_string(VkColorSpaceKHR code) {
    switch(code) {
#define STR(r) case r: return #r
      STR(VK_COLORSPACE_SRGB_NONLINEAR_KHR);
#undef STR
      default:
        return "UNKNOWN COLOR SPACE";
    }
  }

  static const char* type_str(type t) {
    switch (t) {
      case DEBUG:
        return "d";
      case INFO:
        return "i";
      case WARNING:
        return "w";
      case ERROR:
        return "e";
      case FATAL:
        return "fatal";
      default:
        return "?";
    }
  }

  static int type_color(type t) {
    switch (t) {
      case DEBUG:
        return 36;
      case INFO:
        return 32;
      case WARNING:
        return 33;
      case ERROR:
      case FATAL:
        return 31;
      default:
        return 36;
    }
  }

  static FILE *type_stream(type t) {
#ifdef LOG_TO_STD_ERR
    return stderr;
#endif
    switch (t) {
      case DEBUG:
      case INFO:
      case WARNING:
        return stdout;
      case ERROR:
      case FATAL:
        return stderr;
    }
  }

  static std::string color_code(int code) {
    if (!use_color)
      return "";
    char code_str[7];
    snprintf(code_str, sizeof(code_str), "\e[%dm", code);
    return std::string(code_str);
  }

  static std::string strip_file_name(const char* file) {
    std::string striped_path = std::strrchr(file, '/') + 1;
    size_t lastindex = striped_path.find_last_of(".");
    return striped_path.substr(0, lastindex);
  }

  static void log(const char* file, int line, type t, const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_values(file, line, t, format, args);
    va_end(args);
  }

  static void log_short(type t, const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_short_values(t, format, args);
    va_end(args);
  }

  static void log_values(const char* file, int line, type t, const char *format, va_list args) {
    FILE *stream = type_stream(t);
    fprintf(stream, "%s[%s]%s ",
            color_code(type_color(t)).c_str(),
            type_str(t),
            color_code(0).c_str());
    fprintf(stream, "%s:%d | ", strip_file_name(file).c_str(), line);
    vfprintf(stream, format, args);
    fprintf(stream, "\n");
    if (t == FATAL)
      exit(1);
  }

  static void log_short_values(type t, const char *format, va_list args) {
    FILE *stream = type_stream(t);
    fprintf(stream, "%s[%s]%s ",
            color_code(type_color(t)).c_str(),
            type_str(t),
            color_code(0).c_str());
    vfprintf(stream, format, args);
    fprintf(stream, "\n");
  }

  static void log_if(const char* file, int line, type t, bool cond, const char *format, ...) {
    if (!cond)
      return;
    va_list args;
    va_start(args, format);
    log_values(file, line, t, format, args);
    va_end(args);
  }
};
}  // namespace vik

