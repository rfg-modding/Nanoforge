#include "Constants.hlsl"

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

VS_OUTPUT VS(float3 inPos : POSITION, float4 inColor : COLOR)
{
    VS_OUTPUT output;
    output.Pos = mul(float4(inPos.xyz, 1.0f), WVP);
    output.Color = float4(inColor.xyz, 1.0f);

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    return input.Color.xyzw;
}