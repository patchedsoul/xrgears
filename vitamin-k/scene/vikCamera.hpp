/*
 * vitamin-k
 *
 * Copyright 2016 Sascha Willems - www.saschawillems.de
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../render/vikBuffer.hpp"
#include "../render/vikDevice.hpp"

#include "../input/vikInput.hpp"

namespace vik {

struct StereoView {
  glm::mat4 view[2];
};

class Camera {

public:
 Buffer uniformBuffer;

 std::function<void()> view_updated_cb = [](){};

 void set_view_updated_cb(std::function<void()> cb) {
   view_updated_cb = cb;
 }

 struct UBOCamera {
   glm::mat4 projection[2];
   glm::mat4 view[2];
   glm::mat4 skyView[2];
   glm::vec3 position;
 } ubo;

 virtual ~Camera() {
   uniformBuffer.destroy();
 }

 virtual void update_ubo() {
   ubo.projection[0] = matrices.perspective;
   ubo.view[0] = matrices.view;
   ubo.skyView[0] = glm::mat4(glm::mat3(matrices.view));
   ubo.position = position * -1.0f;
   memcpy(uniformBuffer.mapped, &ubo, sizeof(ubo));
 }

 void prepareUniformBuffers(Device *vulkanDevice) {
   vulkanDevice->create_and_map(&uniformBuffer, sizeof(ubo));
 }

 private:
  float fov;
  float znear, zfar;

  void updateViewMatrix() {
    glm::mat4 rotM = glm::mat4();
    glm::mat4 transM;

    rotM = glm::rotate(rotM, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    transM = glm::translate(glm::mat4(), position);

    //if (type == CameraType::firstperson)
    //  matrices.view = rotM * transM;
    //else
      matrices.view = transM * rotM;
  }

 public:
  enum CameraType { lookat, firstperson };
  CameraType type = CameraType::lookat;

  float zoom = 0;
  float rotationSpeed = 1.0f;
  float zoomSpeed = 1.0f;

  //glm::vec3 rotation = glm::vec3();
  glm::vec3 cameraPos = glm::vec3();
  glm::vec2 mousePos;

  struct {
    bool left = false;
    bool right = false;
    bool middle = false;
  } mouseButtons;


  glm::vec3 rotation = glm::vec3();
  glm::vec3 position = glm::vec3();

  //float rotationSpeed = 1.0f;
  float movementSpeed = 1.0f;

  struct {
    glm::mat4 perspective;
    glm::mat4 view;
  } matrices;

  struct {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
  } keys;

  bool moving() {
    return keys.left || keys.right || keys.up || keys.down;
  }

  void setPerspective(float fov, float aspect, float znear, float zfar) {
    this->fov = fov;
    this->znear = znear;
    this->zfar = zfar;
    matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
  }

  void update_aspect_ratio(float aspect) {
    matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
  }

  void setPosition(glm::vec3 position) {
    this->position = position;
    updateViewMatrix();
  }

  void setRotation(glm::vec3 rotation) {
    this->rotation = rotation;
    updateViewMatrix();
  }

  void rotate(glm::vec3 delta) {
    this->rotation += delta;
    updateViewMatrix();
  }

  void translate(glm::vec3 delta) {
    this->position += delta;
    vik_log_d("Translating");
    updateViewMatrix();
  }

  void update_movement(float deltaTime) {
    if (type == CameraType::firstperson) {
      if (moving()) {
        glm::vec3 camFront;
        camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
        camFront.y = sin(glm::radians(rotation.x));
        camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
        camFront = glm::normalize(camFront);

        float moveSpeed = deltaTime * movementSpeed;

        if (keys.up)
          position += camFront * moveSpeed;
        if (keys.down)
          position -= camFront * moveSpeed;
        if (keys.left)
          position -= glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;
        if (keys.right)
          position += glm::normalize(glm::cross(camFront, glm::vec3(0.0f, 1.0f, 0.0f))) * moveSpeed;

        updateViewMatrix();
      }
    }
  }

  void keyboard_key_cb(Input::Key key, bool state) {
    switch (key) {
      case Input::Key::W:
        keys.up = state;
        break;
      case Input::Key::S:
        keys.down = state;
        break;
      case Input::Key::A:
        keys.left = state;
        break;
      case Input::Key::D:
        keys.right = state;
        break;
      default:
        break;
    }
  }

  void pointer_axis_cb(Input::MouseScrollAxis axis, double value) {
    switch (axis) {
      case Input::MouseScrollAxis::X:
        zoom += value * 0.005f * zoomSpeed;
        translate(glm::vec3(0.0f, 0.0f, value * 0.005f * zoomSpeed));
        view_updated_cb();
        break;
      default:
        break;
    }
  }

  void pointer_motion_cb(double x, double y) {
    double dx = mousePos.x - x;
    double dy = mousePos.y - y;

    if (mouseButtons.left) {
      rotation.x += dy * 1.25f * rotationSpeed;
      rotation.y -= dx * 1.25f * rotationSpeed;

      /*
      rotate(glm::vec3(
                      dy * rotationSpeed,
                      -dx * rotationSpeed,
                      0.0f));
      */
      view_updated_cb();
    }

    if (mouseButtons.right) {
      zoom += dy * .005f * zoomSpeed;
      //translate(glm::vec3(-0.0f, 0.0f, dy * .005f * zoomSpeed));
      view_updated_cb();
    }

    if (mouseButtons.middle) {
      cameraPos.x -= dx * 0.01f;
      cameraPos.y -= dy * 0.01f;
      //translate(glm::vec3(-dx * 0.01f, -dy * 0.01f, 0.0f));
      view_updated_cb();
    }
    mousePos = glm::vec2(x, y);
  }

  void pointer_button_cb(Input::MouseButton button, bool state) {
    switch (button) {
      case Input::MouseButton::Left:
        mouseButtons.left = state;
        break;
      case Input::MouseButton::Middle:
        mouseButtons.middle = state;
        break;
      case Input::MouseButton::Right:
        mouseButtons.right = state;
        break;
    }
  }

};
}  // namespace vik
