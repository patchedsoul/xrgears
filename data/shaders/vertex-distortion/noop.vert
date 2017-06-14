#version 450 core

layout (location = 0) in vec3 position;
layout (location = 1) in vec2 uv;

layout (location = 0) out vec2 vUV;

layout (binding = 0) uniform UBOCamera {
	mat4 projectionMatrix;
	mat4 modelViewMatrix;
} uboCamera;

void main() {
  gl_Position = uboCamera.projectionMatrix * uboCamera.modelViewMatrix * vec4(position, 1.0);
  vUV = uv;
}
