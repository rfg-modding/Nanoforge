#include "Constants.hlsl"

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 ZonePos : POSITION0;
    float2 Uv : TEXCOORD;
};

Texture2D Texture0 : register(t0);
Texture2D Texture1 : register(t1);
Texture2D Texture2 : register(t2);
Texture2D Texture3 : register(t3);
Texture2D Texture4 : register(t4);
Texture2D Texture5 : register(t5);
SamplerState Sampler0 : register(s0);
SamplerState Sampler1 : register(s1);
SamplerState Sampler2 : register(s2);
SamplerState Sampler3 : register(s3);
SamplerState Sampler4 : register(s4);
SamplerState Sampler5 : register(s5);

VS_OUTPUT VS(int4 inPos : POSITION)
{
    VS_OUTPUT output;

    //Scale terrain mesh range (-32,768 to 32,767) to zone range (-255.5f to 255.5f as coordinates relative to zone center)
    //In other words: Convert vertices from terrain local coords to zone local coords
    //Range of terrain vertex component values
    float terrainMin = -32768.0f;
    float terrainMax = 32767.0f;
    //Range of values relative to a zones center
    float zoneMin = -255.5f;
    float zoneMax = 255.5f;
    //Ranges
    float terrainRange = terrainMax - terrainMin;
    terrainRange *= 0.9980; //Hack to fill in gaps between zones. May cause object positions to be slightly off
    float zoneRange = zoneMax - zoneMin;

    //Scale position
    float4 posFloat = inPos.xyzw;
    posFloat.x = (((posFloat.x - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.y = (((posFloat.y - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.z = (((posFloat.z - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.w = 1.0f;
    posFloat.y *= 2.0f;
    output.ZonePos = posFloat;
    output.Pos = mul(posFloat, WVP);

    //Calc texture UV
    output.Uv.x = ((posFloat.x + 255.5f) / 511.0f);
    output.Uv.y = ((posFloat.z + 255.5f) / 511.0f);
    output.Uv.x *= 0.9980; //Part of hack to fix terrain gaps. Must scale UVs since we're scaling the terrain size
    output.Uv.y *= 0.9980;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{    
    //Get terrain color from _comb texture and adjust it's brightness
    float4 blendValues = Texture0.Sample(Sampler0, input.Uv);
    float gamma = 0.5f;
    float3 terrainColor = pow(blendValues.xyz, 1.0 / gamma);

    //Get pre-calculated lighting from _ovl texture
    float3 lighting = Texture1.Sample(Sampler1, input.Uv).xyz;;

    //Ambient
    float ambientIntensity = 0.15f;
    float3 ambient = ambientIntensity * terrainColor;

    //Diffuse
    float3 diffuse = lighting * DiffuseColor * DiffuseIntensity * terrainColor;
    diffuse *= 1.5f; //Arbitrary scaling to look similar to high lod terrain. Temporary fix until high lod terrain is fully working

    //Normalized elevation for ShadeMode 0. [-255.5, 255.5] to [0.0, 511.0] to [0.0, 1.0]
    float elevationNormalized = (input.ZonePos.y + 255.5f) / 511.0f;

    if(ShadeMode == 0)
    {
        //Color terrain only by elevation
        return float4(elevationNormalized, elevationNormalized, elevationNormalized, 1.0f);
    }
    else if(ShadeMode == 1)
    {
        //Color terrain with basic lighting
        return float4(ambient + diffuse, 1.0f);
    }
    else
    {
        //If given invalid ShadeMode use elevation coloring (mode 0)
        return float4(elevationNormalized, elevationNormalized, elevationNormalized, 1.0f);
    }
}