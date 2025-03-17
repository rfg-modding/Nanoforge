#version 460
#include "Constants.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inTangent;
layout(location = 2) in ivec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragTangent;

void main()
{
    gl_Position = ubo.proj * ubo.view * objectBuffer.objects[gl_BaseInstance].model * vec4(inPosition, 1.0);
    fragTexCoord = vec2(float(inTexCoord.x), float(inTexCoord.y)) / 1024.0f;

    mat3 normalMatrix = transpose(inverse(mat3(objectBuffer.objects[gl_BaseInstance].model)));
    vec3 tangent = normalize(inTangent.xyz * 2.0f - 1.0f); //Adjust range from [0, 1] to [-1, 1]
    tangent = normalize(normalMatrix * tangent.xyz); //Rotate the normal
    fragTangent = vec4(tangent, 1.0f);
}
