#pragma once

#include <assert.h>

#include "../vkc/vkcCube.hpp"
#include "../vkc/vkcWindow.hpp"
#include "../vkc/vkcRenderer.hpp"

namespace vkc {
class Application {
public:
   Cube model;
   Renderer *renderer;
   Window *display;
   enum window_type mode;

   static inline bool streq(const char *a, const char *b) {
     return strcmp(a, b) == 0;
   }

   Application(uint32_t w, uint32_t h);
   ~Application();

   bool display_mode_from_string(const char *s);
   void parse_args(int argc, char *argv[]);
   void init_display();
   void mainloop();

   int init_display_mode(window_type m);
   void init_display_mode_auto();

};
}
