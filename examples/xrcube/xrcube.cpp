

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fstream>
#include <vector>
#include <exception>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vulkan/vulkan.h>

#include "system/vikApplicationVks.hpp"
#include "render/vikShader.hpp"

class XrCube : public vik::ApplicationVks
{
public:
  XrCube(int argc, char *argv[]) : ApplicationVks(argc, argv) {
    zoom = -2.5f;
    name = "Cube";
    // Values not set here are initialized in the base class constructor
  }

  ~XrCube() {
  }

  // Build separate command buffers for every framebuffer image
  // Unlike in OpenGL all rendering commands are recorded once into command buffers that are then resubmitted to the queue
  // This allows to generate work upfront and from multiple threads, one of the biggest advantages of Vulkan
  void build_command_buffers() {
    for (int32_t i = 0; i < renderer->cmd_buffers.size(); ++i) {
    }

  }

  void init() {
    ApplicationVks::init();

    build_command_buffers();
  }

  virtual void render() {

  }

  virtual void view_changed_cb() {

  }
};

int main(int argc, char *argv[]) {
  XrCube app(argc, argv);
  app.init();
  app.loop();
  return 0;
}
