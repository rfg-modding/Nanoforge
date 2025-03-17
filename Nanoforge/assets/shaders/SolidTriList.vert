#version 460
#include "Constants.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

void main()
{
    gl_Position = ubo.proj * ubo.view * vec4(inPos.xyz, 1.0);
    fragColor = inColor;
}
