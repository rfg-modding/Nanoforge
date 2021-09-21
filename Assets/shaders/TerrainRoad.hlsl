
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
    float Time;
    float2 ViewportDimensions;
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float3 Normal : NORMAL0;
    float3 Tangent : TANGENT0;
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

VS_OUTPUT VS(float4 inPos : POSITION0, float4 inNormal : NORMAL0, float4 inTangent : TANGENT, int2 inUv : TEXCOORD)
{
    VS_OUTPUT output;
    output.Pos = mul(inPos, WVP);
    output.Normal = inNormal.xyz;
    output.Tangent = inTangent.xyz;
    output.Uv = float2(float(inUv.x), float(inUv.y));
    output.Uv.x = output.Uv.x / 1024.0f;
    output.Uv.y = output.Uv.y / 1024.0f;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 color = Texture0.Sample(Sampler0, input.Uv);

    //Sun direction for diffuse lighting
    float3 sunDir = float3(1.0f, -1.0f, 0.0f);

    //Ambient
    float ambientIntensity = 0.05f;
    float3 ambient = float3(ambientIntensity, ambientIntensity, ambientIntensity);

    //Diffuse
    float3 lightDir = normalize(-sunDir);
    float diff = max(dot(input.Normal, lightDir), 0.0f);
    float3 diffuse = diff * color * DiffuseColor * DiffuseIntensity;

    //return float4(input.Normal.xyz, 1.0f);
    //return float4(input.Tangent.xyz, 1.0f);
    return float4(diffuse, 1.0f) + float4(ambient, 1.0f);
}