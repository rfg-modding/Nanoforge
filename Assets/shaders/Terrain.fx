
cbuffer cbPerObject
{
    float4x4 WVP;
};

cbuffer cbPerFrame
{
    float3 ViewPos;
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float3 Normal : NORMAL;
    float4 ZonePos : POSITION0;
};

VS_OUTPUT VS(int4 inPos : POSITION, float3 inNormal : NORMAL)
{
    VS_OUTPUT output;

    //Scale terrain mesh range (-32,768 to 32,767) to zone range (-255.5f to 255.5f as coordinates relative to zone center)
    //In other words: Convert vertices from terrain local coords to zone local coords
    //Range of terrain vertex component values
    float terrainMin = -32768.0f;
    float terrainMax = 32767.0f;
    //Range of values relative to a zones center
    float zoneMin = -255.5f;
    float zoneMax = 255.5f;
    //Ranges
    float terrainRange = terrainMax - terrainMin;
    float zoneRange = zoneMax - zoneMin;
    
    float4 posFloat = float4(inPos.x, inPos.y, inPos.z, inPos.w);
    posFloat.x = (((posFloat.x - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.y = (((posFloat.y - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.z = (((posFloat.z - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.w = 1.0f;
    posFloat.y *= 2.0f;
    output.ZonePos = posFloat;
    output.Pos = mul(posFloat, WVP);
    output.Normal = inNormal;

    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET
{
    float3 sunDir = float3(0.436f, -1.0f, 0.598f);
    float3 sunColor = float3(1.0f, 1.0f, 1.0f);
    float sunIntensity = 0.6f;
    float ambientIntensity = 0.05f;
    float specularStrength = 0.5f;
    float shininess = 32.0f;

    //Diffuse
    float3 lightDir = normalize(-sunDir);
    float diff = max(dot(input.Normal, lightDir), 0.0f);
    float3 diffuse = diff * sunColor;

    //Specular
    float3 viewDir = normalize(ViewPos - input.Pos);
    float3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(viewDir, halfwayDir), 0.0), shininess);
    float3 specular = spec * specularStrength * sunColor;
    
    diffuse *= sunIntensity;
    specular *= sunIntensity;

    //Adjust range from [-255.5, 255.5] to [0.0, 511.0] to [0.0, 1.0] and set color to elevation
    float elevationNormalized = (input.ZonePos.y + 255.5f) / 511.0f;
    float maxElevationFactor = 0.23; //Note: Set this to zero to not factor elevation in
    float elevationFactor = (elevationNormalized - 0.5);
    //Attempt to limit brightness of high elevation terrain
    if(elevationFactor >= maxElevationFactor)
    {
        elevationFactor = maxElevationFactor;
    }

    //Note: Return this line to apply basic lighting + optional coloring by elevation
    return float4(diffuse, 1.0f) + float4(specular, 1.0f) + float4(ambientIntensity, ambientIntensity, ambientIntensity, ambientIntensity) + elevationFactor;

    //Note: Return this line instead of the above to only color terrain by elevation and ignore lighting calculations
    //return float4(elevationNormalized, elevationNormalized, elevationNormalized, 1.0f);
}