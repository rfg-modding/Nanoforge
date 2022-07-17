#include "Constants.hlsl"

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 Uv0 : TEXCOORD0;
    float2 Uv1 : TEXCOORD1;
    float2 Uv2 : TEXCOORD2;
};

Texture2D DiffuseTexture : register(t0);
SamplerState DiffuseSampler : register(s0);
Texture2D SpecularTexture : register(t1);
SamplerState SpecularSampler : register(s1);
Texture2D NormalTexture : register(t2);
SamplerState NormalSampler : register(s2);

VS_OUTPUT VS(float4 inPos : POSITION, float4 inNormal : NORMAL, float4 inTangent : TANGENT, int2 inUv0 : TEXCOORD0, int2 inUv1 : TEXCOORD1, int2 inUv2 : TEXCOORD2)
{
    VS_OUTPUT output;
    output.Pos = mul(inPos, WVP);

    //Adjust range from [0, 1] to [-1, 1]
    output.Normal = normalize(inNormal * 2.0f - 1.0f);
    output.Normal.w = 1.0f;
    output.Normal = mul(Rotation, output.Normal); //Rotate normals based on object rotation //TODO: Do this in all other shaders. Currently rocks are the only things that get rotated, added recently.

    output.Tangent = mul(Rotation, inTangent * 2.0f - 1.0f);

    //Divide by 1024 to normalize
    output.Uv0 = float2(inUv0.x, inUv0.y) / 1024.0f;
    output.Uv1 = float2(inUv1.x, inUv1.y) / 1024.0f;
    output.Uv2 = float2(inUv2.x, inUv2.y) / 1024.0f;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    //Todo: Figure out how second and third UVs should be used. All textures look wrong when sampled with them
    float4 color = DiffuseTexture.Sample(DiffuseSampler, input.Uv0);
    //TODO: Disabled normal map until conversion out of tangent space is added
    float4 normal = input.Normal;//normalize(input.Normal + (NormalTexture.Sample(NormalSampler, input.Uv0) * 2.0 - 1.0));
    float4 specularStrength = SpecularTexture.Sample(SpecularSampler, input.Uv0);

    //Ambient light
    float ambientIntensity = 0.05f;
    float3 ambient = ambientIntensity * color.xyz;

    //Sun direction
    float3 sunPos = float3(30.0f, 30.0f, 30.0f);
    float3 sunDir = normalize(float3(0.0f, 0.0f, 0.0f) - sunPos);

    //Diffuse
    float3 lightDir = normalize(-sunDir);
    float diff = max(dot(lightDir, normal), 0.0f);
    float3 diffuse = diff * color.xyz * DiffuseColor * DiffuseIntensity;

    //Specular highlights
    float3 viewDir = normalize(ViewPos - input.Pos);
    float3 reflectDir = reflect(-lightDir, normal);
    float highlightSize = 8.0f; //Smaller = bigger specular highlights
    float spec = pow(dot(viewDir, reflectDir), highlightSize);
    float3 specular = spec * specularStrength;

    //Final output
    return float4(ambient + diffuse, 1.0f);
}