
cbuffer cbPerObject
{
    float4x4 WVP;
    float4x4 Rotation;
    float4 WorldPosition;
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
    float Time;
    float2 ViewportDimensions;
};

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