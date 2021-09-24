
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
    float4 ZonePos : POSITION0;
    float3 Normal : NORMAL0;
    float2 Uv : TEXCOORD;
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

VS_OUTPUT VS(int2 inPos : POSITION0, float4 inNormal : NORMAL0)
{
    VS_OUTPUT output;

    float posy_scaled = inPos.y + 32768.0f;
    float a = posy_scaled / 256.0f;
    float b = posy_scaled;
    float a_trunc = trunc(a);
    float a_trunc_scaled = a_trunc * 256.0f;

    float4 posScaled = float4(0.0f, 0.0f, 0.0f, 1.0f);
    posScaled.x = b - a_trunc_scaled;
    posScaled.y = inPos.x / 64.0f;
    posScaled.z = a_trunc;
    posScaled.w = 1.0f;

    output.ZonePos = posScaled;
    output.Pos = mul(posScaled, WVP);

    //Calc texture UV relative to start of this map zone
    float3 posWorld = posScaled.xyz + WorldPosition.xyz;
    float x = (posWorld.x / 511.0f);
    float z = (posWorld.z / 511.0f);
    output.Uv = float2(x, z);

    output.Normal = inNormal.xyz;
    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{    
    //Get weight of each texture from alpha00 texture
    float4 blendWeights = Texture0.Sample(Sampler0, input.Uv);

    //Terrain material scaling and offsetting. xy = scale, zw = offset
    float4 layer0ScaleOffset = float4(64.0f, -64.0f, 0.0f, 0.0f);
    float4 layer1ScaleOffset = float4(64.0f, -64.0f, 0.0f, 0.0f);
    float4 layer2ScaleOffset = float4(64.0f, -64.0f, 0.0f, 0.0f);
    float4 layer3ScaleOffset = float4(32.0f, -32.0f, 0.0f, 0.0f);
    
    //Get terrain colors and normals
    float4 color0 = Texture1.Sample(Sampler1, (input.Uv + layer0ScaleOffset.zw) * layer0ScaleOffset.xy);
    float4 normal0 = Texture2.Sample(Sampler2, (input.Uv + layer0ScaleOffset.zw) * layer0ScaleOffset.xy);

    float4 color1 = Texture3.Sample(Sampler3, (input.Uv + layer1ScaleOffset.zw) * layer1ScaleOffset.xy);
    float4 normal1 = Texture4.Sample(Sampler4, (input.Uv + layer1ScaleOffset.zw) * layer1ScaleOffset.xy);

    float4 color2 = Texture5.Sample(Sampler5, (input.Uv + layer2ScaleOffset.zw) * layer2ScaleOffset.xy);
    float4 normal2 = Texture6.Sample(Sampler6, (input.Uv + layer2ScaleOffset.zw) * layer2ScaleOffset.xy);
    
    float4 color3 = Texture7.Sample(Sampler7, (input.Uv + layer3ScaleOffset.zw) * layer3ScaleOffset.xy);
    float4 normal3 = Texture8.Sample(Sampler8, (input.Uv + layer3ScaleOffset.zw) * layer3ScaleOffset.xy);

    normal0 *= 2.0f;
    normal0 -= 1.0f;
    normal0 = normalize(normal0);
    normal1 *= 2.0f;
    normal1 -= 1.0f;
    normal1 = normalize(normal1);
    normal2 *= 2.0f;
    normal2 -= 1.0f;
    normal2 = normalize(normal2);
    normal3 *= 2.0f;
    normal3 -= 1.0f;
    normal3 = normalize(normal3);

    float4 finalColor = color0;
    finalColor += color3 * blendWeights.w;
    finalColor += color1 * blendWeights.z;
    finalColor += color2 * blendWeights.y;
    finalColor += color3 * blendWeights.x;

    float4 finalNormal = float4(input.Normal, 1.0f);// + normal0;
    finalNormal += normal3 * blendWeights.w;
    finalNormal += normal1 * blendWeights.z;
    finalNormal += normal2 * blendWeights.y;
    finalNormal += normal3 * blendWeights.x;
    finalNormal = normalize(finalNormal);

    float gamma = 1.0f;
    finalColor = pow(finalColor.xyzw, float4(1.0 / gamma, 1.0 / gamma, 1.0 / gamma, 1.0 / gamma));

    //Sun direction for diffuse lighting
    float3 sunPos = float3(30.0f, 30.0f, 30.0f);
    float3 sunDir = normalize(float3(0.0f, 0.0f, 0.0f) - sunPos);

    float ambientIntensity = 0.01f;
    float3 ambient = float3(ambientIntensity, ambientIntensity, ambientIntensity);

    //Diffuse
    float3 lightDir = normalize(-sunDir);
    float diff = max(dot(finalNormal, lightDir), 0.0f);
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
        //return float4(input.Normal.xyz, 1.0f);
        return float4(diffuse, 1.0f) + float4(ambient, 1.0f);
    }
    else
    {
        //If given invalid ShadeMode use elevation coloring (mode 0)
        return float4(elevationNormalized, elevationNormalized, elevationNormalized, 1.0f);
    }
}