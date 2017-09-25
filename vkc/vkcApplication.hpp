#pragma once

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <functional>

#include "../vitamin-k/vikApplication.hpp"

#include "../vitamin-k/vikWindowXCBSimple.hpp"
#include "../vitamin-k/vikWindowKMS.hpp"
#include "../vitamin-k/vikWindowWaylandXDG.hpp"
#include "vkcRenderer.hpp"

namespace vkc {

class ApplicationVkc : public vik::Application {
public:
   RendererVkc *renderer;
   vik::Window *window;
   bool quit = false;

   ApplicationVkc(uint32_t w, uint32_t h) {
     renderer = new RendererVkc(w, h);
   }

   ~ApplicationVkc() {
     delete renderer;
   }

   void parse_args(int argc, char *argv[]) {
     if (!settings.parse_args(argc, argv))
       vik_log_f("Invalid arguments.");
     renderer->set_settings(&settings);
   }

   int init_window(vik::Window::window_type m) {
     switch (settings.type) {
       case vik::Window::KMS:
         window = new vik::WindowKMS();
         break;
       case vik::Window::XCB_SIMPLE:
         window = new vik::WindowXCBSimple();
         break;
       case vik::Window::WAYLAND_XDG:
         window = new vik::WindowWaylandXDG();
         break;
       case vik::Window::WAYLAND_LEGACY:
         break;
       case vik::Window::XCB_MOUSE:
         break;
       case vik::Window::AUTO:
         return -1;
     }

     std::function<void()> update_cb = [this]() { update_scene(); };
     std::function<void()> quit_cb = [this]() { quit = true; };

     window->set_update_cb(update_cb);
     window->set_quit_cb(quit_cb);

     int ret = window->init(renderer);

     renderer->init_vulkan("vkcube", window->required_extensions());

     if (!window->check_support(renderer->physical_device))
      vik_log_f("Vulkan not supported on given surface");

     window->init_swap_chain(renderer);

     std::function<void(uint32_t index)> render_cb =
         [this](uint32_t index) { renderer->render(index); };
     renderer->swap_chain->set_render_cb(render_cb);

     init();

     renderer->create_frame_buffers(renderer->swap_chain);

     return ret;
   }

   void init_window_auto() {
     settings.type = vik::Window::WAYLAND_XDG;
     if (init_window(settings.type) == -1) {
       vik_log_e("failed to initialize wayland, falling back to xcb");
       delete(window);
       settings.type = vik::Window::XCB_SIMPLE;
       if (init_window(settings.type) == -1) {
         vik_log_e("failed to initialize xcb, falling back to kms");
         delete(window);
         settings.type = vik::Window::KMS;
         init_window(settings.type);
       }
     }
   }

   void init_window() {
     if (settings.type == vik::Window::AUTO)
       init_window_auto();
     else if (init_window(settings.type) == -1)
       vik_log_f("failed to initialize %s", window->name.c_str());
   }

   void loop() {
     while (!quit)
       window->iterate(renderer);
   }

   virtual void init() = 0;
   //virtual void render(struct RenderBuffer *b) = 0;
   virtual void update_scene() = 0;

};
}
