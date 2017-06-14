#version 450 core

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outColor;

uniform sampler2D image;

void main() {
	outColor = texture(image, inUV);
}
