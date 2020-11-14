
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
    //Todo: See if necessary/good idea to convert to tangent space so things are correct even with rotations. See: https://learnopengl.com/Advanced-Lighting/Normal-Mapping
    VS_OUTPUT output;
    output.Pos = mul(inPos, WVP);
    output.Normal = inNormal;//mul(inNormal, WVP);
    output.Tangent = mul(inTangent, WVP);

    //For some reason need to divide these by 1024 to get proper values. Would've expected 2^16 / 2 to normalize
    output.Uv = float2(float(inUv.x), float(inUv.y));
    output.Uv.x = output.Uv.x / 1024.0f;
    output.Uv.y = output.Uv.y / 1024.0f;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float4 color = DiffuseTexture.Sample(DiffuseSampler, input.Uv);
    float4 normal = NormalTexture.Sample(NormalSampler, input.Uv);
    float4 specularStrength = SpecularTexture.Sample(SpecularSampler, input.Uv);

    //Ambient light
    float ambientIntensity = 0.01f;
    float3 ambient = float3(ambientIntensity, ambientIntensity, ambientIntensity);
    
    //Output color
    float4 outColor = float4(ambient, 1.0f);

    //Sun direction for diffuse lighting
    float3 sunPos = float3(30.0f, 0.0f, 30.0f);
    float3 sunDir = normalize(float3(0.0f, 0.0f, 0.0f) - sunPos);

    //Diffuse light contribution
    float3 lightDir = normalize(-sunDir);
    float diff = max(dot(input.Normal, lightDir), 0.0f);
    float3 diffuse = (diff * color.xyz * DiffuseColor * DiffuseIntensity);
    outColor += float4(diffuse, 1.0f);

    //Specular highlights
    float3 viewDir = normalize(ViewPos - input.Pos);
    float3 reflectDir = reflect(lightDir, input.Normal);
    float spec = dot(normalize(reflectDir), normalize(viewDir));
    if(spec > 0.0f) //Avoid adding dark spots
    {
        spec = pow(spec, 32);
        outColor += spec * specularStrength;// * float4(DiffuseColor, 1.0f);
    }

    //Final output
    return outColor;
}