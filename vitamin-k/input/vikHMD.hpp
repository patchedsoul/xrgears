/*
 * vitamin-k
 *
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gli/gli.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <openhmd/openhmd.h>

#include "../scene/vikCamera.hpp"
#include "../render/vikBuffer.hpp"
#include "../system/vikLog.hpp"
#include "../scene/vikCamera.hpp"
#include "../system/vikSettings.hpp"

namespace vik {
class HMD {
 private:
  ohmd_context* context = nullptr;

 public:
  ohmd_device* device = nullptr;

  HMD(Settings *s) {
    context = ohmd_ctx_create();
    int num_devices = ohmd_ctx_probe(context);

    if (num_devices < 0)
      vik_log_f("Failed to probe HMD: %s", ohmd_ctx_get_error(context));

    ohmd_device_settings* settings = ohmd_device_settings_create(context);

    // If OHMD_IDS_AUTOMATIC_UPDATE is set to 0, ohmd_ctx_update() must be called at least 10 times per second.
    // It is enabled by default.

    int auto_update = 1;
    ohmd_device_settings_seti(settings, OHMD_IDS_AUTOMATIC_UPDATE, &auto_update);

    device = ohmd_list_open_device_s(context, s->hmd, settings);

    if (!device)
      vik_log_f("Failed to open device: %s", ohmd_ctx_get_error(context));

    vik_log_i("Using HMD %d: %s: %s (%s)", s->hmd,
              ohmd_list_gets(context, s->hmd, OHMD_VENDOR),
              ohmd_list_gets(context, s->hmd, OHMD_PRODUCT),
              ohmd_list_gets(context, s->hmd, OHMD_PATH));

    ohmd_device_settings_destroy(settings);

    //print_info(device, s->hmd);
  }

  ~HMD() {
    ohmd_ctx_destroy(context);
  }

  static void enumerate_hmds() {
    ohmd_context *c = ohmd_ctx_create();
    int num_devices = ohmd_ctx_probe(c);
    if (num_devices < 0)
      vik_log_f("Failed to probe HMD: %s", ohmd_ctx_get_error(c));

    vik_log_i("Found %d HMDs.", num_devices);
    for (int i = 0; i < num_devices; i++) {
      vik_log_i_short("%d: %s: %s (%s)", i,
                ohmd_list_gets(c, i, OHMD_VENDOR),
                ohmd_list_gets(c, i, OHMD_PRODUCT),
                ohmd_list_gets(c, i, OHMD_PATH));

      ohmd_device *d = ohmd_list_open_device(c, i);
      print_info(d);
    }
    ohmd_ctx_destroy(c);
  }

  static void print_info(ohmd_device* d) {
    int hmd_w, hmd_h;
    ohmd_device_geti(d, OHMD_SCREEN_HORIZONTAL_RESOLUTION, &hmd_w);
    ohmd_device_geti(d, OHMD_SCREEN_VERTICAL_RESOLUTION, &hmd_h);

    float ipd;
    ohmd_device_getf(d, OHMD_EYE_IPD, &ipd);

    float viewport_scale[2];
    ohmd_device_getf(d, OHMD_SCREEN_HORIZONTAL_SIZE, &(viewport_scale[0]));
    // viewport is half the screen
    viewport_scale[0] /= 2.0f;

    ohmd_device_getf(d, OHMD_SCREEN_VERTICAL_SIZE, &(viewport_scale[1]));

    float distortion_coeffs[4];
    ohmd_device_getf(d, OHMD_UNIVERSAL_DISTORTION_K, &(distortion_coeffs[0]));

    float aberr_scale[3];
    ohmd_device_getf(d, OHMD_UNIVERSAL_ABERRATION_K, &(aberr_scale[0]));

    float sep;
    ohmd_device_getf(d, OHMD_LENS_HORIZONTAL_SEPARATION, &sep);

    float left_lens_center[2];
    ohmd_device_getf(d, OHMD_LENS_VERTICAL_POSITION, &(left_lens_center[1]));

    float right_lens_center[2];
    ohmd_device_getf(d, OHMD_LENS_VERTICAL_POSITION, &(right_lens_center[1]));

    // calculate lens centers
    // assuming the eye separation is the distance between the lense centers
    left_lens_center[0] = viewport_scale[0] - sep/2.0f;
    right_lens_center[0] = sep/2.0f;

    // asume calibration was for lens view to which ever
    // edge of screen is further away from lens center
    float warp_scale = (left_lens_center[0] > right_lens_center[0]) ?
          left_lens_center[0] : right_lens_center[0];

    vik_log_i_short("\tResolution           %dx%d",
        hmd_w,
        hmd_h);

    vik_log_i_short("\tWarp parameters      %.4f %.4f %.4f %.4f",
        distortion_coeffs[0],
        distortion_coeffs[1],
        distortion_coeffs[2],
        distortion_coeffs[3]);
    vik_log_i_short("\tWarp scale           %.4f", warp_scale);
    vik_log_i_short("\tChromatic aberration %.4f %.4f %.4f %.4f",
        aberr_scale[0],
        aberr_scale[1],
        aberr_scale[2]);
    vik_log_i_short("\tLens center left     %.4f %.4f",
        left_lens_center[0],
        left_lens_center[1]);
    vik_log_i_short("\tLens center right    %.4f %.4f",
        right_lens_center[0],
        right_lens_center[1]);
    vik_log_i_short("\tLens separation      %.4f", sep);
    vik_log_i_short("\tIPD                  %.4f", ipd);
    vik_log_i_short("\tViewport scale       %.4f %.4f",
        viewport_scale[0],
        viewport_scale[1]);
  }

  void getTransformation(glm::mat4 *hmdProjectionLeft, glm::mat4 *hmdProjectionRight,
                         glm::mat4 *hmdViewLeft, glm::mat4 *hmdViewRight) {
    ohmd_ctx_update(context);

    float mat[16];
    ohmd_device_getf(device, OHMD_LEFT_EYE_GL_PROJECTION_MATRIX, mat);
    *hmdProjectionLeft = glm::make_mat4(mat);

    ohmd_device_getf(device, OHMD_RIGHT_EYE_GL_PROJECTION_MATRIX, mat);
    *hmdProjectionRight = glm::make_mat4(mat);

    ohmd_device_getf(device, OHMD_LEFT_EYE_GL_MODELVIEW_MATRIX, mat);
    *hmdViewLeft = glm::make_mat4(mat);

    ohmd_device_getf(device, OHMD_RIGHT_EYE_GL_MODELVIEW_MATRIX, mat);
    *hmdViewRight = glm::make_mat4(mat);
  }
};
}  // namespace vik
