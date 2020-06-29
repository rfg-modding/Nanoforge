
cbuffer cbPerObject
{
    float4x4 WVP;
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 ZonePos : POSITION0;
};

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
    float zoneRange = zoneMax - zoneMin;
    
    float4 posFloat = float4(inPos.x, inPos.y, inPos.z, inPos.w);
    posFloat.x = (((posFloat.x - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.y = (((posFloat.y - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.z = (((posFloat.z - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.w = 1.0f;
    posFloat.y *= 2.0f;
    output.ZonePos = posFloat;
    output.Pos = mul(posFloat, WVP);

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    //Adjust range from [-255.5, 255.5] to [0.0, 511.0] and set color to elevation
    float color = (input.ZonePos.y + 255.5f) / 511.0f;
    return float4(color, color, color, 1.0f);
}