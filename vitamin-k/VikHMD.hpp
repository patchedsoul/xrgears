/*
 * XRGears
 *
 * Copyright (C) 2016 Sascha Willems - www.saschawillems.de
 * Copyright (C) 2017 Lubosz Sarnecki <lubosz.sarnecki@collabora.co.uk>
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 *
 * Based on Vulkan Examples written by Sascha Willems
 */

#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gli/gli.hpp>

#include <openhmd/openhmd.h>

#include "../vks/vksCamera.hpp"
#include "../vks/vksBuffer.hpp"

#include "VikCamera.hpp"
#include "VikBuffer.hpp"

class VikHMD {
 private:
  ohmd_context* openHmdContext;

 public:
  ohmd_device* openHmdDevice;

  VikHMD() {
    int hmd_w, hmd_h;

    openHmdContext = ohmd_ctx_create();
    int num_devices = ohmd_ctx_probe(openHmdContext);

    if (num_devices < 0) {
      printf("Failed to probe HMD: %s\n", ohmd_ctx_get_error(openHmdContext));
      return;
    }

    ohmd_device_settings* settings = ohmd_device_settings_create(openHmdContext);

    // If OHMD_IDS_AUTOMATIC_UPDATE is set to 0, ohmd_ctx_update() must be called at least 10 times per second.
    // It is enabled by default.

    int auto_update = 1;
    ohmd_device_settings_seti(settings, OHMD_IDS_AUTOMATIC_UPDATE, &auto_update);

    openHmdDevice = ohmd_list_open_device_s(openHmdContext, 0, settings);
    ohmd_device_geti(openHmdDevice, OHMD_SCREEN_HORIZONTAL_RESOLUTION, &hmd_w);
    ohmd_device_geti(openHmdDevice, OHMD_SCREEN_VERTICAL_RESOLUTION, &hmd_h);
    float ipd;
    ohmd_device_getf(openHmdDevice, OHMD_EYE_IPD, &ipd);
    float viewport_scale[2];
    float distortion_coeffs[4];
    float aberr_scale[3];
    float sep;
    float left_lens_center[2];
    float right_lens_center[2];
    // viewport is half the screen
    ohmd_device_getf(openHmdDevice, OHMD_SCREEN_HORIZONTAL_SIZE, &(viewport_scale[0]));
    viewport_scale[0] /= 2.0f;
    ohmd_device_getf(openHmdDevice, OHMD_SCREEN_VERTICAL_SIZE, &(viewport_scale[1]));
    // distortion coefficients
    ohmd_device_getf(openHmdDevice, OHMD_UNIVERSAL_DISTORTION_K, &(distortion_coeffs[0]));
    ohmd_device_getf(openHmdDevice, OHMD_UNIVERSAL_ABERRATION_K, &(aberr_scale[0]));
    // calculate lens centers (assuming the eye separation is the distance betweenteh lense centers)
    ohmd_device_getf(openHmdDevice, OHMD_LENS_HORIZONTAL_SEPARATION, &sep);
    ohmd_device_getf(openHmdDevice, OHMD_LENS_VERTICAL_POSITION, &(left_lens_center[1]));
    ohmd_device_getf(openHmdDevice, OHMD_LENS_VERTICAL_POSITION, &(right_lens_center[1]));
    left_lens_center[0] = viewport_scale[0] - sep/2.0f;
    right_lens_center[0] = sep/2.0f;
    // asume calibration was for lens view to which ever edge of screen is further away from lens center
    float warp_scale = (left_lens_center[0] > right_lens_center[0]) ? left_lens_center[0] : right_lens_center[0];
    float warp_adj = 1.0f;

    ohmd_device_settings_destroy(settings);

    if (!openHmdDevice) {
      printf("failed to open device: %s\n", ohmd_ctx_get_error(openHmdContext));
      return;
    }

    printf("viewport_scale: [%0.4f, %0.4f]\n", viewport_scale[0], viewport_scale[1]);
    printf("lens separation: %04f\n", sep);
    printf("IPD: %0.4f\n", ipd);
    printf("warp_scale: %0.4f\r\n", warp_scale);
    printf("distoriton coeffs: [%0.4f, %0.4f, %0.4f, %0.4f]\n", distortion_coeffs[0], distortion_coeffs[1], distortion_coeffs[2], distortion_coeffs[3]);
    printf("aberration coeffs: [%0.4f, %0.4f, %0.4f]\n", aberr_scale[0], aberr_scale[1], aberr_scale[2]);
    printf("left_lens_center: [%0.4f, %0.4f]\n", left_lens_center[0], left_lens_center[1]);
    printf("right_lens_center: [%0.4f, %0.4f]\n", right_lens_center[0], right_lens_center[1]);
  }

  ~VikHMD() {
    ohmd_ctx_destroy(openHmdContext);
  }

  void getTransformation(glm::mat4 *hmdProjectionLeft, glm::mat4 *hmdProjectionRight,
                         glm::mat4 *hmdViewLeft, glm::mat4 *hmdViewRight) {
    ohmd_ctx_update(openHmdContext);

    float mat[16];
    ohmd_device_getf(openHmdDevice, OHMD_LEFT_EYE_GL_PROJECTION_MATRIX, mat);
    *hmdProjectionLeft = glm::make_mat4(mat);

    ohmd_device_getf(openHmdDevice, OHMD_RIGHT_EYE_GL_PROJECTION_MATRIX, mat);
    *hmdProjectionRight = glm::make_mat4(mat);

    ohmd_device_getf(openHmdDevice, OHMD_LEFT_EYE_GL_MODELVIEW_MATRIX, mat);
    *hmdViewLeft = glm::make_mat4(mat);

    ohmd_device_getf(openHmdDevice, OHMD_RIGHT_EYE_GL_MODELVIEW_MATRIX, mat);
    *hmdViewRight = glm::make_mat4(mat);
  }
};
