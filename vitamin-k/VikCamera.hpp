#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gli/gli.hpp>

struct UBOCamera {
  glm::mat4 projection[2];
  glm::mat4 view[2];
  glm::mat4 skyView[2];
  glm::vec3 position;
};
