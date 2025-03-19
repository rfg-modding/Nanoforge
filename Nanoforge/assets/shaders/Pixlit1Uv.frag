#version 460
#include "Constants.glsl"

layout(early_fragment_tests) in;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragTangent;
layout(location = 2) flat in int fragObjectIndex;

layout(location = 0) out vec4 outColor;

void main()
{
    MaterialInstance materialInstance = materialBuffer.materials[objectBuffer.objects[fragObjectIndex].MaterialIndex];
    outColor = texture(textures[materialInstance.Texture0], fragTexCoord);
}
