
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
    int MaterialIndex;
};

layout(std140, binding = 1) readonly buffer ObjectBuffer
{
    ObjectData objects[];
} objectBuffer;

struct MaterialInstance
{
    int Texture0;
    int Texture1;
    int Texture2;
    int Texture3;
    int Texture4;
    int Texture5;
    int Texture6;
    int Texture7;
    int Texture8;
    int Texture9;
    int Type; //See the MaterialType enum in C# code
};

layout(std140, binding = 2) readonly buffer MaterialBuffer
{
    MaterialInstance materials[];
} materialBuffer;

layout(binding = 3) uniform sampler2D textures[8192]; //Note: Array size must match TextureManager.MaxTextures