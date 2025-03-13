#version 460

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

void main()
{
    gl_Position = ubo.proj * ubo.view * vec4(inPos.xyz, 1.0);
    fragColor = inColor;
}
