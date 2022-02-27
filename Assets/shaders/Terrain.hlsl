
//Better texturing method with lower performance. Textures don't stretch on cliffs, but they must be sampled 3 times, making performance worse.
#define UseTriplanarMapping true
static const float PI = 3.14159265f;

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
    float3 PosWorld : POSITION1;
    float3 Normal : NORMAL0;
    float2 UvBase : TEXCOORD0;
    float2 Uv0 : TEXCOORD1;
    float2 Uv1 : TEXCOORD2;
    float2 Uv2 : TEXCOORD3;
    float2 Uv3 : TEXCOORD4;
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
    //Note: inPos.xy is 2 16 bit integers. Initial range: [-32768, 32767]
    //      inPos.x contains the final y value, and inPos.y contains the final x and z values

    //Adjust inPos value range and extract upper byte
    int xz = inPos.y + 32768; //Adjust range to [0, 65535]
    int xzUpper = xz & 0xFF00; //Zero the lower byte. Range stays at [0, 65535]
    int xzUpperScaled = xzUpper >> 8; //Left shift 8 bits to adjust range to [0, 256]

    //Final position
    float4 posFinal;
    posFinal.x = xz - xzUpper; //Difference between xz and the upper byte of xz
    posFinal.y = (float)inPos.x / 64.0f; //Divide by 64 to scale y to [-512.0, 512.0]
    posFinal.z = xzUpperScaled;
    posFinal.w = 1.0f;

    output.ZonePos = posFinal;
    output.Pos = mul(posFinal, WVP);

    //Adjust range from [0, 1] to [-1, 1]
    output.Normal = normalize(inNormal.xyz * 2.0f - 1.0f);

    //Calc texture UV relative to start of this map zone
    float3 posWorld = posFinal.xyz + WorldPosition.xyz;
    output.PosWorld = posWorld;
    output.UvBase = posWorld.xz / 511.0f;

    //Adjustable per material scaling to reduce tiling
    float2 material0Scale = float2(128.0f, 128.0f);
    float2 material1Scale = float2(64.0f, 64.0f);
    float2 material2Scale = float2(128.0f, 128.0f);
    float2 material3Scale = float2(16.0f, 16.0f);

    //Calculate material UVs
    output.Uv0 = output.UvBase * material0Scale.xy;
    output.Uv1 = output.UvBase * material1Scale.xy;
    output.Uv2 = output.UvBase * material2Scale.xy;
    output.Uv3 = output.UvBase * material3Scale.xy;

    return output;
}

float4 CalcTerrainColorOld(VS_OUTPUT input, float4 blendWeights)
{
    //Sample terrname_comb.cvbm_pc texture. This is used by the game for minimaps
    //Nanoforge uses it as a temporary way to get colors closer to the games colors
    float4 combColor = Texture1.Sample(Sampler1, input.UvBase);
    float combGamma = 0.45f;
    combColor = pow(combColor.xyzw, 1.0 / combGamma);

    //Get data for terrain materials. 4 of them, each with their own diffuse and normal maps
    //Sample diffuse maps
    float4 color0 = Texture2.Sample(Sampler2, input.Uv0);
    float4 color1 = Texture4.Sample(Sampler4, input.Uv1);
    float4 color2 = Texture6.Sample(Sampler6, input.Uv2);
    float4 color3 = Texture8.Sample(Sampler8, input.Uv3);

    //Calculate final diffuse color
    float4 finalColor = color0 + combColor;
    finalColor += color3 * blendWeights.w;
    finalColor += color1 * blendWeights.z;
    finalColor += color2 * blendWeights.y;
    finalColor += color3 * blendWeights.x;

    return finalColor;
}

float4 CalcTerrainNormalOld(VS_OUTPUT input, float4 blendWeights)
{
    //Sample normal maps
    float4 normal0 = normalize(Texture3.Sample(Sampler3, input.Uv0) * 2 - 1);
    float4 normal1 = normalize(Texture5.Sample(Sampler5, input.Uv1) * 2 - 1);
    float4 normal2 = normalize(Texture7.Sample(Sampler7, input.Uv2) * 2 - 1);
    float4 normal3 = normalize(Texture9.Sample(Sampler9, input.Uv3) * 2 - 1);

    //Calculate final normal
    float4 finalNormal = float4(input.Normal, 1.0f);
    finalNormal += normal3 * blendWeights.w;
    finalNormal += normal1 * blendWeights.z;
    finalNormal += normal2 * blendWeights.y;
    finalNormal += normal3 * blendWeights.x;
    finalNormal = normalize(finalNormal);

    return finalNormal;
}

float4 TriplanarSample(VS_OUTPUT input, Texture2D tex, sampler texSampler, float3 blend, float4 uvScaleTranslate)
{
    //Sample texture from 3 directions & blend the results
    float4 xaxis = tex.Sample(texSampler, input.PosWorld.zy * uvScaleTranslate.xy + uvScaleTranslate.zw);
    float4 yaxis = tex.Sample(texSampler, input.PosWorld.xz * uvScaleTranslate.xy + uvScaleTranslate.zw);
    float4 zaxis = tex.Sample(texSampler, input.PosWorld.yx * uvScaleTranslate.xy + uvScaleTranslate.zw);
    float4 finalTex = xaxis * blend.x + yaxis * blend.y + zaxis * blend.z;
    return finalTex;
}

//Per material scaling to reduce tiling. Only used by triplanar mapping functions. Otherwise the ones in the vertex shader are used.
static float4 material0ScaleTranslate = float4(0.15f.xx, 0.25f.xx);
static float4 material1ScaleTranslate = float4(0.1f.xx, 0.0f.xx);
static float4 material2ScaleTranslate = float4(0.20f.xx, 0.3f.xx);
static float4 material3ScaleTranslate = float4(0.05f.xx, 0.0f.xx);

float4 CalcTerrainColorTriplanar(VS_OUTPUT input, float4 blendWeights)
{
    //Sample terrname_comb.cvbm_pc texture. This is used by the game for minimaps
    //Nanoforge uses it as a temporary way to get colors closer to the games colors
    float4 combColor = Texture1.Sample(Sampler1, input.UvBase);
    float combGamma = 0.5f;
    combColor = pow(combColor.xyzw, 1.0 / combGamma);

    //Calculate triplanar mapping blend
    float3 blend = abs(input.Normal);
    blend = normalize(max(blend, 0.00001)); //Force weights to sum to 1.0
    blend /= (blend.x + blend.y + blend.z);

    //Sample textures using triplanar mapping
    float4 color0 = TriplanarSample(input, Texture2, Sampler2, blend, material0ScaleTranslate);
    float4 color1 = TriplanarSample(input, Texture4, Sampler4, blend, material1ScaleTranslate);
    float4 color2 = TriplanarSample(input, Texture6, Sampler6, blend, material2ScaleTranslate);
    float4 color3 = TriplanarSample(input, Texture8, Sampler8, blend, material3ScaleTranslate);

    //Calculate final diffuse color with blend weights
    float4 finalColor = color0 + combColor;
    finalColor += color3 * blendWeights.w;
    finalColor += color1 * blendWeights.z;
    finalColor += color2 * blendWeights.y;
    finalColor += color3 * blendWeights.x;

    return finalColor;
}

float4 NormalFuckery(float4 n, float4x4 TBN)
{
    n.x = n.z * n.w;
    n.z = sqrt(max(0, 1 - pow(length(n.xy), 2)));
    n = normalize(mul(TBN, n));
    return n;
}

float4 CalcTerrainNormalTriplanar(VS_OUTPUT input, float4 blendWeights, float4x4 TBN)
{
    //Calculate triplanar mapping blend
    float3 blend = abs(input.Normal);
    blend = normalize(max(blend, 0.00001)); //Force weights to sum to 1.0
    blend /= (blend.x + blend.y + blend.z);

    //Sample textures using triplanar mapping
    float4 normal0 = normalize(TriplanarSample(input, Texture3, Sampler3, blend, material0ScaleTranslate) * 2.0 - 1.0);
    float4 normal1 = normalize(TriplanarSample(input, Texture5, Sampler5, blend, material1ScaleTranslate) * 2.0 - 1.0);
    float4 normal2 = normalize(TriplanarSample(input, Texture7, Sampler7, blend, material2ScaleTranslate) * 2.0 - 1.0);
    float4 normal3 = normalize(TriplanarSample(input, Texture9, Sampler9, blend, material3ScaleTranslate) * 2.0 - 1.0);

    normal0 = NormalFuckery(normal0, TBN); //normalize(mul(TBN, normal0));
    normal1 = NormalFuckery(normal1, TBN); //normalize(mul(TBN, normal1));
    normal2 = NormalFuckery(normal2, TBN); //normalize(mul(TBN, normal2));
    normal3 = NormalFuckery(normal3, TBN); //normalize(mul(TBN, normal3));

    //Calculate final normal
    float materialNormalStrength = 1.0f;//0.5f;
    float4 finalNormal = float4(input.Normal, 1.0f) + normal0 * materialNormalStrength;
    finalNormal += normal3 * blendWeights.w * materialNormalStrength;
    finalNormal += normal1 * blendWeights.z * materialNormalStrength;
    finalNormal += normal2 * blendWeights.y * materialNormalStrength;
    finalNormal += normal3 * blendWeights.x * materialNormalStrength;
    finalNormal = normalize(finalNormal);

    return finalNormal;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float3 vertexToEye = ViewPos - input.PosWorld;
    float3 p = -vertexToEye;
    float3 n = input.Normal;
    float2 uv = input.UvBase;

    //Calculate TBN matrix to convert the normal maps from tangent space
    float4x4 TBN;
    {
        //Get edge vectors of the pixel triangle
        float3 dp1 = ddx(p);
        float3 dp2 = ddy(p);
        float2 duv1 = ddx(uv);
        float2 duv2 = ddy(uv);

        //Solve the linear system
        float3 dp2perp = cross(dp2, n);
        float3 dp1perp = cross(n, dp1);
        float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
        float3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    
        //Construct a scale-invariant frame 
        float invmax = rsqrt(max(dot(T,T), dot(B,B)));
        TBN = float4x4(float4(T * invmax, 1.0f), float4(B * invmax, 1.0f), float4(n, 1.0f), float4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    //TBN = transpose(TBN);
    
    //Get weight of each texture from alpha00 texture
    float4 blendWeights = Texture0.Sample(Sampler0, input.UvBase);
    float4 finalColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
    float4 finalNormal = float4(1.0f, 1.0f, 1.0f, 1.0f);

    if (UseTriplanarMapping)
    {
        finalColor = CalcTerrainColorTriplanar(input, blendWeights);
        finalNormal = CalcTerrainNormalTriplanar(input, blendWeights, TBN);
    }
    else
    {
        finalColor = CalcTerrainColorOld(input, blendWeights);
        finalNormal = CalcTerrainNormalOld(input, blendWeights);
    }

    //Sun direction for diffuse lighting
    float3 sunDir = float3(0.5f, -0.7f, -0.7f);
    
    //Adjust sun direction with timer
    //  float timerLength = 10.0f;
    //  float sunTimer = ((Time % timerLength) / timerLength); //In range [0.0, 1.0] over the course of timerLength seconds
    //  float sunAngle = sunTimer * (2 * PI); //Goes from [0, 2PI] radians in timerLength seconds
    //  float sunX = (cos(sunAngle));
    //  float sunZ = (sin(sunAngle));
    //  sunDir.x = sunX;
    //  sunDir.z = sunZ;

    //Ambient
    float ambientIntensity = 0.1f;
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
        //return float4(sunX, 0.0f, sunZ, 1.0f);
        return float4(ambient + diffuse, 1.0f);
        //return float4((Time % 10.0f) / 10.0f, 0.0f, 0.0f, 1.0f);
        //return float4(finalNormal.xyz, 1.0f);
    }
    else
    {
        //If given invalid ShadeMode use elevation coloring (mode 0)
        return float4(elevationNormalized, elevationNormalized, elevationNormalized, 1.0f);
    }
}