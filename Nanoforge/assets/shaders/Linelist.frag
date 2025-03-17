#version 460
#include "Constants.glsl"

layout(location = 0) in float fragSize;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = fragColor;
}
