
layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

struct ObjectData
{
    mat4 model;
    vec4 worldPos;
};

layout(std140, binding = 11) readonly buffer ObjectBuffer
{
    ObjectData objects[];
} objectBuffer;