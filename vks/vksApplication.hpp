/*
* Vulkan Example base class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <sys/stat.h>

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include <iostream>
#include <chrono>
#include <string>
#include <array>
#include <vector>

#include "vksTools.hpp"
#include "vksInitializers.hpp"

#include "vksCamera.hpp"
#include "vksRenderer.hpp"
#include "vksTimer.hpp"

#include "vikApplication.hpp"

namespace vks {
class Window;

class Application : public vik::Application {

 public:
  Renderer *renderer;
  Camera camera;
  vik::Window *window;

  bool prepared = false;
  bool viewUpdated = false;
  bool resizing = false;

  float zoom = 0;
  float rotationSpeed = 1.0f;
  float zoomSpeed = 1.0f;

  glm::vec3 rotation = glm::vec3();
  glm::vec3 cameraPos = glm::vec3();
  glm::vec2 mousePos;

  std::string title = "Vulkan Example";
  std::string name = "vulkanExample";

  bool quit = false;
  struct {
    bool left = false;
    bool right = false;
    bool middle = false;
  } mouseButtons;

  explicit Application();
  virtual ~Application();

  virtual void render() = 0;
  virtual void viewChanged();
  virtual void keyPressed(uint32_t);
  virtual void buildCommandBuffers();
  virtual void getEnabledFeatures();
  virtual void prepare();

  void loop();
  void update_camera(float frame_time);
  void parse_arguments(int argc, char *argv[]);

  void windowResize();

  void check_view_update();


};
}
