#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <cstring>
#include <iostream>

#define vik_log(...) vks::Log::log(__FILE__, __LINE__, __VA_ARGS__)
#define vik_log_d(...) vik_log(vks::Log::DEBUG, __VA_ARGS__)
#define vik_log_i(...) vik_log(vks::Log::INFO, __VA_ARGS__)
#define vik_log_w(...) vik_log(vks::Log::WARNING, __VA_ARGS__)
#define vik_log_e(...) vik_log(vks::Log::ERROR, __VA_ARGS__)
#define vik_log_f(...) vik_log(vks::Log::FATAL, __VA_ARGS__)
#define vik_log_f_if(...) vks::Log::log_fatal_if(__FILE__, __LINE__, __VA_ARGS__)

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

  static std::string type_str(type t) {
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

  static std::string color_code(int code) {
    if (!use_color)
      return "";

    char *code_str = (char *) malloc(7);
    sprintf(code_str, " \e[%dm", code);
    return std::string(code_str);
  }

  static std::string strip_file_name(const std::string& file) {
    std::string striped_path = std::strrchr(file.c_str(), '/') + 1;
    size_t lastindex = striped_path.find_last_of(".");
    return striped_path.substr(0, lastindex);
  }

  static void log(const std::string& file, int line, type t, const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_values(file, line, t, format, args);
    va_end(args);
  }

  static void log_values(const std::string& file, int line, type t, const char *format, va_list args) {
    fprintf(stdout, "[%s%s%s] ", color_code(type_color(t)).c_str(), type_str(t).c_str(), color_code(0).c_str());
    fprintf(stdout, "%s:%d | ", strip_file_name(file).c_str(), line);
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
    if (t == FATAL)
      exit(1);
  }

  static void debug_values(const char *format, va_list args) {
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
  }

  static void error_values(const char *format, va_list args) {
    fprintf(stderr, "[%se%s] ", color_code(type_color(ERROR)).c_str(), color_code(0).c_str());
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
  }

  static void fatal_values(const char *format, va_list args) {
    error_values(format, args);
    exit(1);
  }

  static void error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    error_values(format, args);
    va_end(args);
  }

  static void debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    debug_values(format, args);
    va_end(args);
  }

  static void fatal(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fatal_values(format, args);
    va_end(args);
  }

  static void log_fatal_if(const std::string& file, int line, bool cond, const char *format, ...) {
    if (!cond)
      return;

    va_list args;
    va_start(args, format);
    log_values(file, line, FATAL, format, args);
    va_end(args);
    exit(1);
  }

  static void fatal_if(bool cond, const char *format, ...) {
    if (!cond)
      return;

    va_list args;
    va_start(args, format);
    fatal_values(format, args);
    va_end(args);
  }
};
}

