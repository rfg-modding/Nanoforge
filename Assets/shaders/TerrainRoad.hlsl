
cbuffer cbPerObject
{
    float4x4 Model;
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
    float4 ZonePos : POSITION0;
    float3 Tangent : TANGENT0;
    float3 Normal : NORMAL0;
    float2 UvBase : TEXCOORD0;
    float2 Uv0 : TEXCOORD1;
    float2 Uv1 : TEXCOORD2;
    float2 Uv2 : TEXCOORD3;
    float2 Uv3 : TEXCOORD4;
    float2 UvWorld : TEXCOORD5;
};

Texture2D Texture0 : register(t0);
Texture2D Texture1 : register(t1);
Texture2D Texture2 : register(t2);
Texture2D Texture3 : register(t3);
Texture2D Texture4 : register(t4);
Texture2D Texture5 : register(t5);
Texture2D Texture6 : register(t6);
Texture2D Texture7 : register(t7);
Texture2D Texture8 : register(t8);
Texture2D Texture9 : register(t9);
SamplerState Sampler0 : register(s0);
SamplerState Sampler1 : register(s1);
SamplerState Sampler2 : register(s2);
SamplerState Sampler3 : register(s3);
SamplerState Sampler4 : register(s4);
SamplerState Sampler5 : register(s5);
SamplerState Sampler6 : register(s6);
SamplerState Sampler7 : register(s7);
SamplerState Sampler8 : register(s8);
SamplerState Sampler9 : register(s9);

VS_OUTPUT VS(float4 inPos : POSITION0, float4 inNormal : NORMAL0, float4 inTangent : TANGENT, int2 inUv : TEXCOORD)
{
    VS_OUTPUT output;
    output.Pos = mul(inPos, WVP);
    output.ZonePos = inPos;
    output.Normal = normalize(inNormal.xyz * 2.0f - 1.0f);
    output.Tangent = inTangent.xyz;

    float3 posWorld = inPos.xyz + WorldPosition.xyz;
    output.UvWorld = posWorld.xz / 511.0f;
    output.UvBase = inUv / 1024.0f; //Default UV adjustment
    output.UvBase = output.UvBase / 1.9 + 0.465; //Arbitrary adjustments to correct road UVs

    //Adjustable per material scaling to reduce tiling
    float2 material0Scale = float2(128.0f, 128.0f);
    float2 material1Scale = float2(64.0f, 64.0f);
    float2 material2Scale = float2(128.0f, 128.0f);
    float2 material3Scale = float2(16.0f, 16.0f);

    //Calculate material UVs
    output.Uv0 = output.UvWorld * material0Scale.xy;
    output.Uv1 = output.UvWorld * material1Scale.xy;
    output.Uv2 = output.UvWorld * material2Scale.xy;
    output.Uv3 = output.UvWorld * material3Scale.xy;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    //Get weight of each texture from alpha00 texture
    float4 blendWeights = Texture0.Sample(Sampler0, input.UvWorld);
    
    //Get data for terrain materials. 4 of them, each with their own diffuse and normal maps
    //Sample diffuse maps
    float4 colorBase = Texture8.Sample(Sampler8, input.UvBase);
    float4 color0 = Texture2.Sample(Sampler2, input.Uv0);
    float4 color1 = Texture4.Sample(Sampler4, input.Uv1);
    float4 color2 = Texture6.Sample(Sampler6, input.Uv2);
    float4 color3 = Texture8.Sample(Sampler8, input.Uv3);

    //Sample normal maps
    float4 normal0 = normalize(Texture3.Sample(Sampler3, input.Uv0) * 2 - 1);
    float4 normal1 = normalize(Texture5.Sample(Sampler5, input.Uv1) * 2 - 1);
    float4 normal2 = normalize(Texture7.Sample(Sampler7, input.Uv2) * 2 - 1);
    float4 normal3 = normalize(Texture9.Sample(Sampler9, input.Uv3) * 2 - 1);

    //Calculate final diffuse color
    float4 finalColor = colorBase;
    //finalColor += color1 * blendWeights.z; //Currently excluded since it makes oasis roads look wrong
    finalColor += color2 * blendWeights.y;
    finalColor += color3 * blendWeights.x;

    //Calculate final normal
    float4 finalNormal = float4(input.Normal, 1.0f);
    //finalNormal += normal1 * blendWeights.z; //Currently excluded since it makes oasis roads look wrong
    finalNormal += normal2 * blendWeights.y;
    finalNormal += normal3 * blendWeights.x;
    finalNormal = normalize(finalNormal);

    //Sun direction for diffuse lighting
    float3 sunDir = float3(0.0f, -1.0f, -1.0f);

    //Ambient
    float ambientIntensity = 0.15f;
    float3 ambient = ambientIntensity * finalColor;

    //Diffuse
    float3 lightDir = -normalize(sunDir);
    float diff = max(dot(lightDir, finalNormal), 0.0f);
    float3 diffuse = diff * finalColor * DiffuseColor * DiffuseIntensity;

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