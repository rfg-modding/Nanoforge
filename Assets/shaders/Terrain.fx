
cbuffer cbPerObject
{
    float4x4 WVP;
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float3 Normal : NORMAL;
    float4 ZonePos : POSITION0;
};

VS_OUTPUT VS(int4 inPos : POSITION, float3 inNormal : NORMAL)
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
    float zoneRange = zoneMax - zoneMin;
    
    float4 posFloat = float4(inPos.x, inPos.y, inPos.z, inPos.w);
    posFloat.x = (((posFloat.x - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.y = (((posFloat.y - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.z = (((posFloat.z - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.w = 1.0f;
    posFloat.y *= 2.0f;
    output.ZonePos = posFloat;
    output.Pos = mul(posFloat, WVP);
    output.Normal = inNormal;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float3 sunPos = float3(1000.0f, 1000.0f, 1000.0f);
    float3 sunDir = float3(0.0f, 0.0f, 0.0f) - sunPos;
    float3 sunColor = float3(1.0f, 1.0f, 1.0f);
    float sunIntensity = 0.7f;
    float ambientIntensity = 0.1f;
    float specularStrength = 0.5f;
    float shininess = 32.0f;

    float3 lightDir = normalize(-sunDir);
    float diff = max(dot(input.Normal, lightDir), 0.0f);
    float3 diffuse = diff * sunColor;
    diffuse *= sunIntensity;

    return float4(diffuse, 1.0f) + float4(ambientIntensity, ambientIntensity, ambientIntensity, ambientIntensity);
    //return float4(input.Normal.xyz, 1.0f);

    //Adjust range from [-255.5, 255.5] to [0.0, 511.0] and set color to elevation
    //float color = (input.ZonePos.y + 255.5f) / 511.0f;
    //return float4(color, color, color, 1.0f);
}