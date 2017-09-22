#pragma once

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "vikApplication.hpp"

#include "vkcWindow.hpp"

namespace vkc {

class Renderer;

class Application : public vik::Application {
public:
   Renderer *renderer;
   Window *window;
   bool quit = false;

   Application(uint32_t w, uint32_t h);
   ~Application();

   void parse_args(int argc, char *argv[]);
   void init_window();
   void loop();

   int init_window(vik::Window::window_type m);
   void init_window_auto();

   virtual void init() = 0;
   //virtual void render(struct RenderBuffer *b) = 0;
   virtual void update_scene() = 0;

};
}
