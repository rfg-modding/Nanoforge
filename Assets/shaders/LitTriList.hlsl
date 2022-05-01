
cbuffer cbPerObject
{
    float4x4 WVP;
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
    
    //Sun direction
    float3 sunDir = float3(0.3f, -1.0f, -1.0f);

    //Diffuse
    float3 lightDir = -normalize(sunDir);
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