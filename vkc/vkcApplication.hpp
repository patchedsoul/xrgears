#pragma once

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../vitamin-k/vikApplication.hpp"

#include "../vkc/vkcWindow.hpp"

namespace vkc {

class Renderer;

class Application : public vik::Application {
public:
   Renderer *renderer;
   Window *window;
   enum window_type type;

   bool quit = false;

   static inline bool streq(const char *a, const char *b) {
     return strcmp(a, b) == 0;
   }

   Application(uint32_t w, uint32_t h);
   ~Application();

   bool window_type_from_string(const char *s);
   void parse_args(int argc, char *argv[]);
   void init_window();
   void loop();

   int init_window(window_type m);
   void init_window_auto();

   virtual void init() = 0;
   //virtual void render(struct RenderBuffer *b) = 0;
   virtual void update_scene() = 0;

};
}
