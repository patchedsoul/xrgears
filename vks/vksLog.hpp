#pragma once

#include <stdio.h>
#include <stdlib.h>

enum log_type {
  LOG_DEBUG = 0,
  LOG_INFO,
  LOG_WARNING,
  LOG_ERROR,
  LOG_FATAL
};

static const char* color_red = " \e[31m";
static const char* color_reset = " \e[0m";

static void log_debug_values(const char *format, va_list args) {
  vfprintf(stdout, format, args);
  fprintf(stdout, "\n");
}

static void log_error_values(const char *format, va_list args) {
  fprintf(stderr, "[%se%s] ", color_red, color_reset);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
}

static void log_fatal_values(const char *format, va_list args) {
  log_error_values(format, args);
  exit(1);
}

static void log_error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_error_values(format, args);
  va_end(args);
}

static void log_debug(const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_debug_values(format, args);
  va_end(args);
}

static void log_fatal(const char *format, ...) {
  va_list args;
  va_start(args, format);
  log_fatal_values(format, args);
  va_end(args);
}

static void log_fatal_if(bool cond, const char *format, ...) {
  if (!cond)
    return;

  va_list args;
  va_start(args, format);
  log_fatal_values(format, args);
  va_end(args);
}


