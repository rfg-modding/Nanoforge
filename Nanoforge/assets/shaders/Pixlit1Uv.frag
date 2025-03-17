#version 460
#include "Constants.glsl"

layout(early_fragment_tests) in;

//layout(binding = 0) uniform sampler texSampler;
//layout(binding = 1) uniform texture2D textures[8192];
layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragTangent;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(texSampler, fragTexCoord);
}
