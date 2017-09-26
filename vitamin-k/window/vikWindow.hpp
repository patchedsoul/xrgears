#pragma once

#include <functional>

#include "render/vikSwapChain.hpp"
#include "render/vikRenderer.hpp"
#include "input/vikInput.hpp"

namespace vik {
class Window {
public:
  std::string name;

  std::function<void()> init_cb;
  std::function<void()> update_cb;
  std::function<void()> quit_cb;

  std::function<void(SwapChain *sc)> recreate_frame_buffers_cb;
  std::function<void()> recreate_swap_chain_drm_cb;

  std::function<void(double x, double y)> pointer_motion_cb;
  std::function<void(Input::MouseButton button, bool state)> pointer_button_cb;
  std::function<void(Input::MouseScrollAxis axis, double value)> pointer_axis_cb;
  std::function<void(Input::Key key, bool state)> keyboard_key_cb;

  std::function<void(uint32_t width, uint32_t height)> configure_cb;
  std::function<void(uint32_t width, uint32_t height)> dimension_cb;

  Window() {}
  ~Window() {}

  void set_init_cb(std::function<void()> cb) {
    init_cb = cb;
  }

  void set_update_cb(std::function<void()> cb) {
    update_cb = cb;
  }

  void set_quit_cb(std::function<void()> cb) {
    quit_cb = cb;
  }

  void set_recreate_swap_chain_vk_cb(std::function<void(SwapChain *sc)> cb) {
    recreate_frame_buffers_cb = cb;
  }

  void set_recreate_swap_chain_drm_cb(std::function<void()> cb) {
    recreate_swap_chain_drm_cb = cb;
  }

  void set_pointer_motion_cb(std::function<void(double x, double y)> cb) {
    pointer_motion_cb = cb;
  }

  void set_pointer_button_cb(std::function<void(Input::MouseButton button,
                                                bool state)> cb) {
    pointer_button_cb = cb;
  }

  void set_pointer_axis_cb(std::function<void(
                             Input::MouseScrollAxis axis,
                             double value)> cb) {
    pointer_axis_cb = cb;
  }

  void set_keyboard_key_cb(std::function<void(
                             Input::Key key, bool state)> cb) {
    keyboard_key_cb = cb;
  }

  void set_configure_cb(std::function<void(uint32_t width, uint32_t height)> cb) {
    configure_cb = cb;
  }

  void set_dimension_cb(std::function<void(uint32_t width, uint32_t height)> cb) {
    dimension_cb = cb;
  }

  virtual SwapChain* get_swap_chain() = 0;

  virtual int init(uint32_t width, uint32_t height, bool fullscreen) = 0;
  virtual void iterate(VkQueue queue, VkSemaphore semaphore) = 0;
  virtual const std::vector<const char*> required_extensions() = 0;
  virtual void init_swap_chain(VkInstance instance, VkPhysicalDevice physical_device,
                               VkDevice device, uint32_t width, uint32_t height) = 0;
  virtual void update_window_title(const std::string& title) = 0;
  virtual VkBool32 check_support(VkPhysicalDevice physical_device) = 0;
};
}
