#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <cstring>
#include <iostream>

#define LOG_TO_STD_ERR 0

#define vik_log(...) vks::Log::log(__FILE__, __LINE__, __VA_ARGS__)
#define vik_log_d(...) vik_log(vks::Log::DEBUG, __VA_ARGS__)
#define vik_log_i(...) vik_log(vks::Log::INFO, __VA_ARGS__)
#define vik_log_w(...) vik_log(vks::Log::WARNING, __VA_ARGS__)
#define vik_log_e(...) vik_log(vks::Log::ERROR, __VA_ARGS__)
#define vik_log_f(...) vik_log(vks::Log::FATAL, __VA_ARGS__)
#define vik_log_if(...) vks::Log::log_if(__FILE__, __LINE__, __VA_ARGS__)
#define vik_log_f_if(...) vik_log_if(vks::Log::FATAL, __VA_ARGS__)
#define vik_log_e_if(...) vik_log_if(vks::Log::ERROR, __VA_ARGS__)

// Macro to check and display Vulkan return results
#define vik_log_check(f) {\
  VkResult res = (f);\
  vik_log_f_if(res != VK_SUCCESS, "VkResult is %s", vks::tools::errorString(res).c_str());\
}

namespace vks {
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

  static const char* type_str(type t) {
    switch(t) {
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
    }
  }

  static int type_color(type t) {
    switch(t) {
      case DEBUG:
        return 36;
      case INFO:
        return 32;
      case WARNING:
        return 33;
      case ERROR:
      case FATAL:
        return 31;
    }
  }

  static FILE *type_stream(type t) {
#ifdef LOG_TO_STD_ERR
    return stderr;
#endif
    switch(t) {
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

  static void log_if(const char* file, int line, type t, bool cond, const char *format, ...) {
    if (!cond)
      return;
    va_list args;
    va_start(args, format);
    log_values(file, line, t, format, args);
    va_end(args);
  }

};
}

