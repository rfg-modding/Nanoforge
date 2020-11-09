
cbuffer cbPerObject
{
    float4x4 WVP;
};

cbuffer cbPerFrame
{
    //Position of the camera
    float3 ViewPos;
    int Padding0; //Padding since DirectX::XMVector is really 16 bytes

    //Color of the diffuse light
    float3 DiffuseColor;
    int Padding1; //Padding since DirectX::XMVector is really 16 bytes

    //Intensity of diffuse light 
    float DiffuseIntensity;
    //Bias of elevation coloring in ShadeMode 1. If 0 elevation won't effect color
    float ElevationFactorBias;
    //Shade mode 0: Color terrain only by elevation
    //Shade mode 1: Color terrain with basic lighting + option elevation coloring
    int ShadeMode;
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
    terrainRange *= 0.9980; //Hack to fill in gaps between zones. May cause object positions to be slightly off
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
    //Sun direction for diffuse lighting
    float3 sunDir = float3(0.436f, -1.0f, 0.598f);

    //Diffuse
    float3 lightDir = normalize(-sunDir);
    float diff = max(dot(input.Normal, lightDir), 0.0f);
    float3 diffuse = diff * DiffuseColor * DiffuseIntensity;

    //Adjust range from [-255.5, 255.5] to [0.0, 511.0] to [0.0, 1.0] and set color to elevation
    float elevationNormalized = (input.ZonePos.y + 255.5f) / 511.0f;
    float elevationFactor = (elevationNormalized - 0.5);
    elevationFactor *= ElevationFactorBias; //Scale elevation factor to make high terrain brightless less harsh

    if(ShadeMode == 0)
    {
        //Color terrain only by elevation
        return float4(elevationNormalized, elevationNormalized, elevationNormalized, 1.0f);
    }
    else if(ShadeMode == 1)
    {
        //Color terrain with basic lighting + optional elevation coloring
        return float4(diffuse, 1.0f) + elevationFactor;
    }
    else
    {
        //If given invalid ShadeMode use elevation coloring (mode 0)
        return float4(elevationNormalized, elevationNormalized, elevationNormalized, 1.0f);
    }
}