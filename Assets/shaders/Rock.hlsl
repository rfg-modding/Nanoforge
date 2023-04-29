#include "Constants.hlsl"

/*Note: This is just Pixlit1UvNmap without a specular texture*/

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 Uv : TEXCOORD;
};

Texture2D DiffuseTexture : register(t0);
SamplerState DiffuseSampler : register(s0);
Texture2D SpecularTexture : register(t1);
SamplerState SpecularSampler : register(s1);
Texture2D NormalTexture : register(t2);
SamplerState NormalSampler : register(s2);

VS_OUTPUT VS(float4 inPos : POSITION, float4 inNormal : NORMAL, float4 inTangent : TANGENT, int2 inUv : TEXCOORD)
{
    VS_OUTPUT output;
    output.Pos = mul(inPos, WVP);

    //Adjust range from [0, 1] to [-1, 1]
    output.Normal = normalize(inNormal * 2.0f - 1.0f);
    output.Normal.w = 1.0f;
    output.Normal = mul(Rotation, output.Normal); //Rotate normals based on object rotation //TODO: Do this in all other shaders. Currently rocks are the only things that get rotated, added recently.

    output.Tangent = mul(Rotation, inTangent);

    //Divide by 1024 to normalize
    output.Uv = float2(inUv.x, inUv.y) / 1024.0f;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 color = DiffuseTexture.Sample(DiffuseSampler, input.Uv);
    float4 normal = (input.Normal) + (NormalTexture.Sample(NormalSampler, input.Uv) * 2.0f - 1.0f);

    //Ambient light
    float ambientIntensity = 0.10f;
    float3 ambient = ambientIntensity * color;

    //Sun direction
    float3 sunDir = float3(0.3f, -1.0f, -1.0f);

    //Diffuse
    float3 lightDir = normalize(-sunDir);
    float diff = max(dot(lightDir, normal), 0.0f);
    float3 diffuse = diff * color.xyz * DiffuseColor * DiffuseIntensity;

    //Final output
    return float4(ambient + diffuse, 1.0f);
}