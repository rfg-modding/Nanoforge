
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
};

VS_OUTPUT VS(float4 inPos : POSITION, float4 inNormal : NORMAL)
{
    VS_OUTPUT output;
    output.Pos = mul(inPos, WVP);
    output.Normal = inNormal, WVP;
    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 color = 1.0f;
    float4 normal = input.Normal;

    //Ambient light
    float ambientIntensity = 0.01f;
    float3 ambient = float3(ambientIntensity, ambientIntensity, ambientIntensity);
    
    //Output color
    float4 outColor = float4(ambient, 1.0f);

    //Sun direction for diffuse lighting
    float3 sunDir = float3(1.0f, -1.0f, 0.0f);

    //Diffuse light contribution
    float3 lightDir = normalize(-sunDir);
    float diff = max(dot(input.Normal, lightDir), 0.0f);
    float3 diffuse = (diff * color.xyz * DiffuseColor * DiffuseIntensity);
    outColor += float4(diffuse, 1.0f);

    //Final output
    return outColor;
}