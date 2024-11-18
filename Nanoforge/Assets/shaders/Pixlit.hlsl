#include "Constants.hlsl"

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 Normal : NORMAL;
};

VS_OUTPUT VS(float4 inPos : POSITION, float4 inNormal : NORMAL)
{
    VS_OUTPUT output;
    output.Pos = mul(inPos, WVP);
    
    //Adjust range from [0, 1] to [-1, 1]
    output.Normal = normalize(inNormal * 2.0f - 1.0f);
    output.Normal.w = 1.0f;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 color = 1.0f;
    float4 normal = input.Normal;

    //Ambient light
    float ambientIntensity = 0.01f;
    float3 ambient = float3(ambientIntensity, ambientIntensity, ambientIntensity);

    //Diffuse
    float3 lightDir = -normalize(SunDirection);
    float diff = max(dot(lightDir, normal), 0.0f);
    float3 diffuse = diff * color.xyz * DiffuseColor * DiffuseIntensity;

    //Final output
    return float4(ambient + diffuse, 1.0f);
}