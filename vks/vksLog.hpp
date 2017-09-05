#pragma once

#include <stdio.h>
#include <stdlib.h>

#define vik_log_debug    Logger(__FILE__,__LINE__, Logger::LOG_DEBUG)
#define vik_log_error    Logger(__FILE__,__LINE__, Logger::Error)
#define vik_log_warning  Logger(__FILE__,__LINE__, Logger::Warning)
#define vik_log_info     Logger(__FILE__,__LINE__, Logger::Info)
#define vik_log_fatal    Logger(__FILE__,__LINE__, Logger::Fatal)

static const char* color_red = " \e[31m";
static const char* color_reset = " \e[0m";

namespace vks {
class Log {

public:

  enum log_type {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_FATAL
  };

  static void debug_values(const char *format, va_list args) {
    vfprintf(stdout, format, args);
    fprintf(stdout, "\n");
  }

  static void error_values(const char *format, va_list args) {
    fprintf(stderr, "[%se%s] ", color_red, color_reset);
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

