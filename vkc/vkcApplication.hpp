#pragma once

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../vkc/vkcWindow.hpp"

namespace vkc {

class Renderer;

class Application {
public:
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

   virtual void init() = 0;
   virtual void render(struct RenderBuffer *b) = 0;

};
}
