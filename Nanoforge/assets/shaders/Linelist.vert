#version 460

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

layout(location = 0) in vec4 inPosAndSize;
layout(location = 1) in vec4 inColor;

layout(location = 0) out float fragSize;
layout(location = 1) out vec4 fragColor;

void main()
{
    vec3 pos = inPosAndSize.xyz;
    float size = inPosAndSize.w;
    gl_Position = ubo.proj * ubo.view * vec4(pos, 1.0);
    fragSize = size;
    fragColor = inColor;
    
    //TODO: Add geometry shader to turn the lines into quads. Use size field to determine line width
}
