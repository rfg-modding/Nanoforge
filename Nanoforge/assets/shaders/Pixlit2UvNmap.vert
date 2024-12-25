#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

//Note: Do not to exceed 128 bytes for this data. The spec requires 128 minimum. Can't guarantee that more will be available.
//      If more data is needed some other approach will have to be taken like having 1 UBO per RenderObject
layout(push_constant) uniform ObjectPushConstants {
    mat4 model;
} objectData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in ivec2 inTexCoord;
layout(location = 4) in ivec2 inTexCoord2;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragTangent;
layout(location = 2) out vec4 fragNormal;

void main() {
    gl_Position = ubo.proj * ubo.view * objectData.model * vec4(inPosition, 1.0);
    fragTexCoord = vec2(float(inTexCoord.x), float(inTexCoord.y)) / 1024.0f;
    fragTangent = inTangent;
    fragNormal = inNormal;
}
