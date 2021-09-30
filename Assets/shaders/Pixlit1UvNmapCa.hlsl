
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
    float4 Tangent : TANGENT;
    float4 BoneWeight : BLENDWEIGHT;
    uint4 BoneIndex : BLENDINDEX;
    float2 Uv : TEXCOORD;
};

Texture2D DiffuseTexture : register(t0);
SamplerState DiffuseSampler : register(s0);
Texture2D SpecularTexture : register(t1);
SamplerState SpecularSampler : register(s1);
Texture2D NormalTexture : register(t2);
SamplerState NormalSampler : register(s2);

VS_OUTPUT VS(float4 inPos : POSITION, float4 inNormal : NORMAL, float4 inTangent : TANGENT, float4 inBoneWeight : BLENDWEIGHT, 
             uint4 inBoneIndex : BLENDINDEX, int2 inUv : TEXCOORD)
{
    VS_OUTPUT output;
    output.Pos = mul(inPos, WVP);

    //Adjust range from [0, 1] to [-1, 1]
    output.Normal = normalize(inNormal * 2.0f - 1.0f);
    output.Normal.w = 1.0f;

    output.Tangent = mul(inTangent, WVP);

    //Divide by 1024 to normalize
    output.Uv = float2(inUv.x, inUv.y) / 1024.0f;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 color = DiffuseTexture.Sample(DiffuseSampler, input.Uv);
    float4 normal = normalize(input.Normal + (NormalTexture.Sample(NormalSampler, input.Uv) * 2.0 - 1.0));
    float4 specularStrength = SpecularTexture.Sample(SpecularSampler, input.Uv);

    //Ambient light
    float ambientIntensity = 0.05f;
    float3 ambient = ambientIntensity * color;
    
    //Sun direction
    float3 sunPos = float3(30.0f, 30.0f, 30.0f);
    float3 sunDir = normalize(float3(0.0f, 0.0f, 0.0f) - sunPos);

    //Diffuse
    float3 lightDir = normalize(-sunDir);
    float diff = max(dot(lightDir, normal), 0.0f);
    float3 diffuse = diff * color.xyz * DiffuseColor * DiffuseIntensity;

    //Specular highlights
    float3 viewDir = normalize(ViewPos - input.Pos);
    float3 reflectDir = reflect(-lightDir, normal);
    float highlightSize = 8.0f; //Smaller = bigger specular highlights
    float spec = pow(dot(viewDir, reflectDir), highlightSize);
    float3 specular = spec * specularStrength;

    //Final output
    return float4(ambient + diffuse + specular, 1.0f);
}