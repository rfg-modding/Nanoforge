
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
SamplerState Sampler0 : register(s0);
SamplerState Sampler1 : register(s1);
SamplerState Sampler2 : register(s2);
SamplerState Sampler3 : register(s3);
SamplerState Sampler4 : register(s4);
SamplerState Sampler5 : register(s5);

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

    //Calc texture UV
    output.Uv.x = ((posScaled.x + 255.5f) / 511.0f);
    output.Uv.y = ((posScaled.z + 255.5f) / 511.0f);

    output.Normal = inNormal.xyz;
    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{    
    //Get terrain color from _comb texture and adjust it's brightness
    float4 blendValues = Texture0.Sample(Sampler0, input.Uv);
    float gamma = 0.45f;
    float3 terrainColor = pow(blendValues.xyz, float3(1.0 / gamma, 1.0 / gamma, 1.0 / gamma));

    //Sun direction for diffuse lighting
    float3 sunDir = float3(1.0f, -1.0f, 0.0f);

    float ambientIntensity = 0.05f;
    float3 ambient = float3(ambientIntensity, ambientIntensity, ambientIntensity);

    //Diffuse
    float3 lightDir = normalize(-sunDir);
    float diff = max(dot(input.Normal, lightDir), 0.0f);
    float3 diffuse = diff * terrainColor * DiffuseColor * DiffuseIntensity;

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