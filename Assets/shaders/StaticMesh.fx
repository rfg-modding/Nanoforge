
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
    float4 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float2 Uv : UV;
};

VS_OUTPUT VS(float4 inPos : POSITION, float4 inNormal : NORMAL, float4 inTangent : TANGENT, float2 inUv : UV)
{
    VS_OUTPUT output;
    output.Pos = mul(inPos, WVP);
    output.Normal = inNormal;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    //Sun direction for diffuse lighting
    float3 sunDir = float3(0.436f, -1.0f, 0.598f);
    float ambientIntensity = 0.4f;
    float3 ambient = float3(ambientIntensity, ambientIntensity, ambientIntensity);

    //Diffuse
    float3 lightDir = normalize(-sunDir);
    float diff = max(dot(input.Normal, lightDir), 0.0f);
    float3 diffuse = diff * DiffuseColor * DiffuseIntensity;

    //Todo: Add specular highlights
    //Color with basic diffuse + ambient lighting
    return float4(diffuse, 1.0f) + float4(ambient, 1.0f);
}