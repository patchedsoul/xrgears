/*
 * xrgears
 *
 * Copyright 2017 Philipp Zabel
 * Copyright 2017-2018 Collabora Ltd.
 *
 * Authors: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
 * SPDX-License-Identifier: MIT
 */

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_multiview : enable
#extension GL_ARB_shading_language_420pack : enable

// TODO: Don't use hard coded config
float aspect_x_over_y = 0.8999999761581421;
float grow_for_undistort = 0.6000000238418579;

vec2 undistort_r2_cutoff = vec2(1.11622154712677, 1.101870775222778);

vec2 center[2] = vec2[](
  vec2(0.08946027017045266, -0.009002181016260827),
  vec2(-0.08933516629552526, -0.006014565287238661)
);

vec3 coeffs[2][3] = {
  // left
  {
    // green
    vec3(-0.188236068524731, -0.221086205321053, -0.2537849057915209),
    // blue
    vec3(-0.07316590815739493, -0.02332400789561968, 0.02469959434698275),
    // red
    vec3(-0.02223805567703767, -0.04931309279533211, -0.07862881939243466),
  },
  // right
  {
    // green
    vec3(-0.1906209981894497, -0.2248896677207884, -0.2721364516782803),
    // blue
    vec3(-0.07346071902951497, -0.02189527566250131, 0.0581378652359256),
    // red
    vec3(-0.01755850332081247, -0.04517245633373419, -0.0928909347763)
  }
};

layout (binding = 0) uniform sampler2D texSampler;

layout (location = 0) in vec2 inStereoUV;
layout (location = 1) in vec2 inMonoUV;
layout (location = 2) flat in int inViewIndex;

layout (location = 0) out vec4 outColor;

void main() {
  const int i = inViewIndex;

  // one eye per pass
  // const vec2 factor = 0.5 / (1.0 + grow_for_undistort)
  //                   * vec2(1.0, aspect_x_over_y);

  // two eyes per pass
  const vec2 factor = 0.5 / (1.0 + grow_for_undistort)
                    * vec2(0.5, aspect_x_over_y);

  vec2 texCoord = 2.0 * inMonoUV - vec2(1.0);

  texCoord.y /= aspect_x_over_y;
  texCoord -= center[i];

  float r2 = dot(texCoord, texCoord);

  vec3 d_inv = ((r2 * coeffs[i][2] + coeffs[i][1])
               * r2 + coeffs[i][0])
               * r2 + vec3(1.0);

  const vec3 d = 1.0 / d_inv;

  // one eye per pass
  // const vec2 offset = vec2(0.5);

  // two eyes per pass
  const vec2 offset = vec2(0.25 + i * 0.5, 0.5);

  vec2 tcR = offset + (texCoord * d.r + center[i]) * factor;
  vec2 tcG = offset + (texCoord * d.g + center[i]) * factor;
  vec2 tcB = offset + (texCoord * d.b + center[i]) * factor;

  vec3 color = vec3(
        texture(texSampler, tcR).r,
        texture(texSampler, tcG).g,
        texture(texSampler, tcB).b);

  if (r2 > undistort_r2_cutoff[i])
    color *= 0.125;

  outColor = vec4(color, 1.0);

  // Debug
  // outColor = vec4(inMonoUV, 0, 1.0);
  // outColor = texture(texSampler, texCoord);
}
