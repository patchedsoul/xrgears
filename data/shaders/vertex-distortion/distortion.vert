#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 uv;

layout (location = 0) out vec2 vUV;


layout (binding = 0) uniform UBOCamera {
	mat4 projectionMatrix;
	mat4 modelViewMatrix;
} uboCamera;


layout (binding = 1) uniform UBODistortion {
  float coefficients[12];
  float maxFovSquared;
  vec2 fovOffset;
  vec2 fovScale;
  mat4 viewportTransform;
} uboDistortion;

// Returns a scalar to distort a point; computed in reverse via the polynomial approximation:
//   r' = 1 + Σ_i (uboDistortion.coefficients[i] rSquared^(i+1))  i=[0..11]
// where rSquared is the squared radius of an undistorted point in tan-angle space.
// See {@link Distortion} for more information.
float DistortionFactor(float rSquared) {
  float ret = 0.0;
  rSquared = min(uboDistortion.maxFovSquared, rSquared);
  ret = rSquared * (ret + uboDistortion.coefficients[11]);
  ret = rSquared * (ret + uboDistortion.coefficients[10]);
  ret = rSquared * (ret + uboDistortion.coefficients[9]);
  ret = rSquared * (ret + uboDistortion.coefficients[8]);
  ret = rSquared * (ret + uboDistortion.coefficients[7]);
  ret = rSquared * (ret + uboDistortion.coefficients[6]);
  ret = rSquared * (ret + uboDistortion.coefficients[5]);
  ret = rSquared * (ret + uboDistortion.coefficients[4]);
  ret = rSquared * (ret + uboDistortion.coefficients[3]);
  ret = rSquared * (ret + uboDistortion.coefficients[2]);
  ret = rSquared * (ret + uboDistortion.coefficients[1]);
  ret = rSquared * (ret + uboDistortion.coefficients[0]);
  return ret + 1.0;
}

// Given a point in clip space, distort the point according to the coefficients stored in
// uboDistortion.coefficients and the field of view (FOV) specified in uboDistortion.fovOffset and
// uboDistortion.fovScale.
// Returns the distorted point in clip space, with its Z untouched.
vec4 Distort(vec4 point) {
  // Put point into normalized device coordinates (NDC), [(-1, -1, -1) to (1, 1, 1)].
  vec3 pointNdc = point.xyz / point.w;
  // Throw away the Z coordinate and map the point to the unit square, [(0, 0) to (1, 1)].
  vec2 pointUnitSquare = (pointNdc.xy + vec2(1.0)) / 2.0;
  // Map the point into FOV tan-angle space.
  vec2 pointTanAngle = pointUnitSquare * uboDistortion.fovScale - uboDistortion.fovOffset;
  float radiusSquared = dot(pointTanAngle, pointTanAngle);
  float distortionFactor = DistortionFactor(radiusSquared);
  //'float distortionFactor = 2.0;
  vec2 distortedPointTanAngle = pointTanAngle * distortionFactor;
  // Reverse the mappings above to bring the distorted point back into NDC space.
  vec2 distortedPointUnitSquare = (distortedPointTanAngle + uboDistortion.fovOffset)
  / uboDistortion.fovScale;
  vec3 distortedPointNdc = vec3(distortedPointUnitSquare * 2.0 - vec2(1.0), pointNdc.z);
  // Convert the point into clip space before returning in case any operations are done after.
  return vec4(distortedPointNdc, 1.0) * point.w;
}


vec4 Viewport(vec4 point) {
  return uboDistortion.viewportTransform * point;
}


void main() {
  // Pass through texture coordinates to the fragment shader.
  vUV = uv;

  // Here, we want to ensure that we are using an undistorted projection
  // matrix. By setting isUndistorted: true in the WebVRManager, we
  // guarantee this.
  vec4 pos = uboCamera.projectionMatrix * uboCamera.modelViewMatrix * vec4(position, 1.0);

  // First we apply distortion.
  vec4 distortedPos = Distort(pos);

  // Then constrain in a viewport.
  gl_Position = Viewport(distortedPos);
}
