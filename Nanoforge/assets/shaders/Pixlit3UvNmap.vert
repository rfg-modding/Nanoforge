#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
} ubo;

//Note: Do not to exceed 128 bytes for this data. The spec requires 128 minimum. Can't guarantee that more will be available.
//      If more data is needed some other approach will have to be taken like having 1 UBO per RenderObject
layout(push_constant) uniform ObjectPushConstants
{
    mat4 model;
    vec4 cameraPos;
} objectData;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in ivec2 inTexCoord;
layout(location = 4) in ivec2 inTexCoord2;
layout(location = 5) in ivec2 inTexCoord3;

layout(location = 0) out vec3 vertexWorldPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec4 fragTangent;
layout(location = 3) out vec4 fragNormal;

void main()
{
    vertexWorldPos = (objectData.model * vec4(inPosition, 1.0f)).xyz;
    gl_Position = ubo.proj * ubo.view * objectData.model * vec4(inPosition, 1.0);
    fragTexCoord = vec2(float(inTexCoord.x), float(inTexCoord.y)) / 1024.0f;
    
    mat3 normalMatrix = transpose(inverse(mat3(objectData.model)));
    
    //TODO: Figure out what is wrong with the vertex tangents. They seem to be messed up.
    //TODO: Try regenerating them during mesh import and see if that fixes issues using normal maps
    vec3 tangent = normalize(inTangent.xyz * 2.0f - 1.0f); //Adjust range from [0, 1] to [-1, 1]
    tangent = normalize(normalMatrix * tangent.xyz); //Rotate the tangent
    fragTangent = vec4(tangent, 1.0f);
    
    vec3 normal = normalize(inNormal.xyz * 2.0f - 1.0f); //Adjust range from [0, 1] to [-1, 1]
    normal = normalize(normalMatrix * normal.xyz); //Rotate the normal
    fragNormal = vec4(normal, 1.0f);
}
