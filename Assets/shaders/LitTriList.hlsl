#include "Constants.hlsl"

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 Normal : NORMAL;
    float4 Color : COLOR;
};

VS_OUTPUT VS(float3 inPos : POSITION, float3 inNormal : NORMAL, float4 inColor : COLOR)
{
    VS_OUTPUT output;
    output.Pos = mul(float4(inPos.xyz, 1.0f), WVP);
    output.Normal = float4(inNormal.xyz, 1.0f);
    output.Color = float4(inColor.xyz, 1.0f);

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 color = input.Color;
    float4 normal = input.Normal;

    //Ambient light
    float ambientIntensity = 0.1f;
    float3 ambient = float3(ambientIntensity, ambientIntensity, ambientIntensity);

    //Diffuse
    float3 lightDir = -normalize(SunDirection);
    float diff = max(dot(lightDir, normal), 0.0f);
    float3 diffuse = diff * color.xyz * DiffuseColor * DiffuseIntensity;

    //Final output
    return float4(ambient + diffuse, 1.0f);

    //float4 normal = input.Normal * 2.0f - 1.0f;
    //return input.Color.xyzw;
    //return input.Normal.xyzw;
    //return normal;
    //return float4(1.0f, 0.0f, 0.0f, 1.0f);
}