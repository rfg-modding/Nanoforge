#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

//Note: Do not to exceed 128 bytes for this data. The spec requires 128 minimum. Can't guarantee that more will be available.
//      If more data is needed some other approach will have to be taken like having 1 UBO per RenderObject
layout(push_constant) uniform ObjectPushConstants
{
    mat4 model;
    vec4 worldPos;
} objectData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inTangent;
layout(location = 2) in ivec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragTangent;

void main()
{
    gl_Position = ubo.proj * ubo.view * objectData.model * vec4(inPosition, 1.0);
    fragTexCoord = vec2(float(inTexCoord.x), float(inTexCoord.y)) / 1024.0f;

    mat3 normalMatrix = transpose(inverse(mat3(objectData.model)));
    vec3 tangent = normalize(inTangent.xyz * 2.0f - 1.0f); //Adjust range from [0, 1] to [-1, 1]
    tangent = normalize(normalMatrix * tangent.xyz); //Rotate the normal
    fragTangent = vec4(tangent, 1.0f);
}
