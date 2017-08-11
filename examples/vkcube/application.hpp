#pragma once

#include <assert.h>

#include "cube.hpp"
#include "display.hpp"
#include "VikRenderer.hpp"

class CubeApplication {
public:
   Cube model;
   CubeRenderer *renderer;
   VikDisplayMode *display;
   enum display_mode_type mode;

   static inline bool streq(const char *a, const char *b) {
     return strcmp(a, b) == 0;
   }

   CubeApplication(uint32_t w, uint32_t h);
   ~CubeApplication();

   bool display_mode_from_string(const char *s);
   void parse_args(int argc, char *argv[]);
   void init_display();
   void mainloop();
};


