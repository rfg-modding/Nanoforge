#version 460
#include "Constants.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inNormal;

layout(location = 0) out vec3 vertexZonePos;
layout(location = 1) out vec3 vertexWorldPos;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec4 fragNormal;
layout(location = 4) out int fragObjectIndex;

void main()
{
    vertexZonePos = inPosition;
    gl_Position = ubo.proj * ubo.view * objectBuffer.objects[gl_BaseInstance].model * vec4(inPosition, 1.0);

    //Note: We don't bother using the normal matrix here for this material since we don't ever rotate terrain
    vec3 normal = normalize(inNormal.xyz * 2.0f - 1.0f); //Adjust range from [0, 1] to [-1, 1]
    fragNormal = vec4(normal, 1.0f);
    
    //Calc texture UV relative to start of this map zone
    vec3 posWorld = inPosition.xyz + objectBuffer.objects[gl_BaseInstance].worldPos.xyz;
    vertexWorldPos = posWorld;
    fragTexCoord = posWorld.xz / 511.0f;

    fragObjectIndex = gl_BaseInstance;
}