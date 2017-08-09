#pragma once

#include <stdio.h>
#include <stdlib.h>

static void
failv(const char *format, va_list args)
{
   vfprintf(stderr, format, args);
   fprintf(stderr, "\n");
   exit(1);
}

static void
fail(const char *format, ...)
{
   va_list args;

   va_start(args, format);
   failv(format, args);
   va_end(args);
}

static void
fail_if(int cond, const char *format, ...)
{
   va_list args;

   if (!cond)
      return;

   va_start(args, format);
   failv(format, args);
   va_end(args);
}
