
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
    float2 Uv0 : TEXCOORD0;
    float2 Uv1 : TEXCOORD1;
    float2 Uv2 : TEXCOORD2;
};

Texture2D DiffuseTexture : register(t0);
SamplerState DiffuseSampler : register(s0);
Texture2D SpecularTexture : register(t1);
SamplerState SpecularSampler : register(s1);
Texture2D NormalTexture : register(t2);
SamplerState NormalSampler : register(s2);

VS_OUTPUT VS(float4 inPos : POSITION, float4 inNormal : NORMAL, float4 inTangent : TANGENT, int2 inUv0 : TEXCOORD0, int2 inUv1 : TEXCOORD1, int2 inUv2 : TEXCOORD2)
{
    //Todo: See if necessary/good idea to convert to tangent space so things are correct even with rotations. See: https://learnopengl.com/Advanced-Lighting/Normal-Mapping
    VS_OUTPUT output;
    output.Pos = mul(inPos, WVP);
    output.Normal = inNormal, WVP;
    output.Tangent = mul(inTangent, WVP);

    //For some reason need to divide these by 1024 to get proper values. Would've expected 2^16 / 2 to normalize
    output.Uv0 = float2(float(inUv0.x), float(inUv0.y));
    output.Uv0.x = output.Uv0.x / 1024.0f;
    output.Uv0.y = output.Uv0.y / 1024.0f;
    output.Uv1 = float2(float(inUv1.x), float(inUv1.y));
    output.Uv1.x = output.Uv1.x / 1024.0f;
    output.Uv1.y = output.Uv1.y / 1024.0f;
    output.Uv2 = float2(float(inUv2.x), float(inUv2.y));
    output.Uv2.x = output.Uv2.x / 1024.0f;
    output.Uv2.y = output.Uv2.y / 1024.0f;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    //Todo: Determine if how second uv should be used. Unsure so just using it for normal map
    float4 color = DiffuseTexture.Sample(DiffuseSampler, input.Uv0);
    float4 normal = NormalTexture.Sample(NormalSampler, input.Uv1);
    float4 specularStrength = SpecularTexture.Sample(SpecularSampler, input.Uv2);

    //Sun direction for diffuse lighting
    float3 sunPos = float3(50.0f, 100.0f, 100.0f);
    float3 sunDir = normalize(float3(0.0f, 0.0f, 0.0f) - sunPos);

    //Ambient light
    float ambientIntensity = 0.01f;
    float3 ambient = float3(ambientIntensity, ambientIntensity, ambientIntensity);

    //Diffuse light contribution
    float3 lightDir = normalize(-sunDir);
    float diff = max(dot(normal, lightDir), 0.0f);
    float3 diffuse = (diff * color.xyz * DiffuseColor * DiffuseIntensity) ;

    //Todo: Fix specular highlights. They don't show up
    //Specular highlights
    float3 viewDir = normalize(ViewPos - input.Pos);
    float3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(viewDir, halfwayDir), 0.0), 32);
    float3 specular = spec * specularStrength * DiffuseColor;

    //Color with basic diffuse + specular + ambient lighting
    return float4(diffuse, 1.0f) + float4(specular, 1.0f) + float4(ambient, 1.0f);
}