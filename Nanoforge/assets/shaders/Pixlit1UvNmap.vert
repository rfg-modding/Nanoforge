#version 460

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

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in ivec2 inTexCoord;

layout(location = 0) out vec3 vertexWorldPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 fragTangent;
layout(location = 3) out vec4 fragNormal;

void main()
{
    vertexWorldPos = (objectBuffer.objects[gl_BaseInstance].model * vec4(inPosition, 1.0f)).xyz;
    gl_Position = ubo.proj * ubo.view * objectBuffer.objects[gl_BaseInstance].model * vec4(inPosition, 1.0);
    fragTexCoord = vec2(float(inTexCoord.x), float(inTexCoord.y)) / 1024.0f;

    mat3 normalMatrix = transpose(inverse(mat3(objectBuffer.objects[gl_BaseInstance].model)));

    //TODO: Figure out what is wrong with the vertex tangents. They seem to be messed up.
    //TODO: Try regenerating them during mesh import and see if that fixes issues using normal maps
    vec3 tangent = normalize(inTangent.xyz * 2.0f - 1.0f); //Adjust range from [0, 1] to [-1, 1]
    tangent = normalize(normalMatrix * tangent.xyz); //Rotate the tangent
    fragTangent = vec4(tangent, 1.0f);

    vec3 normal = normalize(inNormal.xyz * 2.0f - 1.0f); //Adjust range from [0, 1] to [-1, 1]
    normal = normalize(normalMatrix * normal.xyz); //Rotate the normal
    fragNormal = vec4(normal, 1.0f);
}