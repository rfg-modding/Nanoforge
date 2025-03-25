#version 460
#include "Constants.glsl"
#include "Colorspace.glsl"

layout(early_fragment_tests) in;

layout(location = 0) in vec3 fragWorldPosTransformed;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 fragTangent;
layout(location = 3) in vec4 fragNormal;
layout(location = 4) flat in int fragObjectIndex;
layout(location = 5) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

vec4 CalculateDefaultMaterialLighting();
vec4 CalculateHighLodTerrainMaterialLighting();
vec4 CalcTerrainColorTriplanar(vec4 blendWeights, MaterialInstance materialInstance);
vec4 CalcTerrainNormalTriplanar(vec4 blendWeights, MaterialInstance materialInstance);
vec4 NormalToObjectSpace(vec3 normal, mat3 tangentSpace);
vec4 TriplanarSample(sampler2D tex, vec3 blend, vec4 uvScaleTranslate);

void main()
{
    MaterialInstance materialInstance = materialBuffer.materials[objectBuffer.objects[fragObjectIndex].MaterialIndex];
    if (materialInstance.Type == 0) //Default
    {
        outColor = CalculateDefaultMaterialLighting();
    }
    else if (materialInstance.Type == 1) //High lod terrain
    {
        outColor = CalculateHighLodTerrainMaterialLighting();
    }
    else //Invalid
    {
        outColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);    
    }
}

//TODO: Move each materials lighting function into a separate file.
vec4 CalculateDefaultMaterialLighting()
{
    //TODO: Put these in the per frame UBO
    vec3 sunPos = vec3(1000.0f, 1000.0f, 1000.0f);
    vec3 sunDirection = normalize(vec3(0.0f, 0.0f, 0.0f) - sunPos);
    vec3 sunlightColor = vec3(1.0f, 1.0f, 1.0f);
    float sunlightIntensity = 1.0f;

    MaterialInstance materialInstance = materialBuffer.materials[objectBuffer.objects[fragObjectIndex].MaterialIndex];
    vec4 diffuseColor = texture(textures[materialInstance.Texture0], fragTexCoord);

    //TODO: Change to also use normal maps in addition to vertex normals when available. First need to fix or regenerate vertex tangents
    vec3 normal = fragNormal.xyz;

    //Diffuse
    float ambientIntensity = 0.07f; //Control the minimum darkness so things faces that aren't in the direction of the sun are still visible
    vec3 lightDir = -normalize(sunDirection);
    float amountLit = max(dot(lightDir, normal.xyz), ambientIntensity);
    vec3 diffuse = amountLit * diffuseColor.xyz * sunlightColor * sunlightIntensity;

    //Specular highlights
    vec3 viewDir = normalize(ubo.cameraPos.xyz - fragWorldPosTransformed);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    vec3 reflectDir = reflect(-lightDir, normal.xyz);
    float highlightSize = 64.0f; //Smaller = bigger specular highlights
    //float spec = pow(max(dot(viewDir, reflectDir), 0.0f), highlightSize);
    float spec = pow(max(dot(normal.xyz, halfwayDir), 0.0f), highlightSize);
    vec4 specularStrength = texture(textures[materialInstance.Texture2], fragTexCoord);
    vec3 specular = spec * specularStrength.xyz;

    return vec4(diffuse + specular, 1.0f);
}

vec4 CalculateHighLodTerrainMaterialLighting()
{
    vec3 sunPos = vec3(1000.0f, 1000.0f, 1000.0f);//TODO: Define this in per-frame UBO
    vec3 sunDirection = normalize(vec3(0.0f, 0.0f, 0.0f) - sunPos);
    vec3 sunlightColor = vec3(1.0f, 1.0f, 1.0f);
    float sunlightIntensity = 1.0f;
    MaterialInstance materialInstance = materialBuffer.materials[objectBuffer.objects[fragObjectIndex].MaterialIndex];

    vec3 posWorld = fragWorldPos;
    vec2 uv = posWorld.xz / 511.0f;
    
    //Get weight of each texture from alpha00 texture and make sure they sum to 1.0
    vec4 blendWeightsBase = texture(textures[materialInstance.Texture0], uv);
    vec4 blendWeights = blendWeightsBase;
    blendWeights = normalize(max(blendWeights, 0.001));
    blendWeights /= (blendWeights.x + blendWeights.y + blendWeights.z + blendWeights.w);

    //Triplanar mapping used to fix stretched textures on steep faces
    //These functions also handle blending together the 4 texture using the splatmap (sampler0) for weights
    vec4 finalColor = CalcTerrainColorTriplanar(blendWeights, materialInstance);
    vec4 finalNormal = CalcTerrainNormalTriplanar(blendWeights, materialInstance);

    //The current approach results in very grey/colorless looking terrain. Saturate it a bit
    vec3 finalColorHsl = rgb2hsl(finalColor.xyz);
    finalColorHsl.y *= 1.3f;
    finalColor = vec4(hsl2rgb(finalColorHsl), 1.0f);

    //Diffuse
    float ambientIntensity = 0.07f;//Control the minimum darkness so things faces that aren't in the direction of the sun are still visible
    vec3 lightDir = -normalize(sunDirection);
    float amountLit = max(dot(lightDir, finalNormal.xyz), ambientIntensity);
    vec3 diffuse = amountLit * finalColor.xyz * sunlightColor * sunlightIntensity;

    //return blendWeightsBase;//vec4(diffuse, 1.0f);
    return vec4(diffuse, 1.0f);
}

//Per material scaling to reduce tiling
vec4 material0ScaleTranslate = vec4(0.15f.xx, 0.25f.xx);
vec4 material1ScaleTranslate = vec4(0.1f.xx, 0.0f.xx);
vec4 material2ScaleTranslate = vec4(0.20f.xx, 0.3f.xx);
vec4 material3ScaleTranslate = vec4(0.05f.xx, 0.0f.xx);

vec4 CalcTerrainColorTriplanar(vec4 blendWeights, MaterialInstance materialInstance)
{
    //Calculate triplanar mapping blend
    vec3 tpmBlend = abs(fragNormal.xyz);
    tpmBlend = normalize(max(tpmBlend, 0.00001));//Force weights to sum to 1.0
    tpmBlend /= (tpmBlend.x + tpmBlend.y + tpmBlend.z);

    //Sample textures using triplanar mapping
    vec4 color0 = TriplanarSample(textures[materialInstance.Texture2], tpmBlend, material0ScaleTranslate);
    vec4 color1 = TriplanarSample(textures[materialInstance.Texture4], tpmBlend, material1ScaleTranslate);
    vec4 color2 = TriplanarSample(textures[materialInstance.Texture6], tpmBlend, material2ScaleTranslate);
    vec4 color3 = TriplanarSample(textures[materialInstance.Texture8], tpmBlend, material3ScaleTranslate);

    //Calculate final diffuse color with blend weights
    vec4 finalColor = color0;
    finalColor += color3 * blendWeights.w;
    finalColor += color1 * blendWeights.z;
    finalColor += color2 * blendWeights.y;
    finalColor += color3 * blendWeights.x;

    return finalColor;
}

vec4 CalcTerrainNormalTriplanar(vec4 blendWeights, MaterialInstance materialInstance)
{
    //Calculate triplanar mapping blend
    vec3 tpmBlend = abs(fragNormal.xyz);
    tpmBlend = normalize(max(tpmBlend, 0.00001));//Force weights to sum to 1.0
    tpmBlend /= (tpmBlend.x + tpmBlend.y + tpmBlend.z);

    //Sample textures using triplanar mapping
    vec4 normal0 = normalize(TriplanarSample(textures[materialInstance.Texture3], tpmBlend, material0ScaleTranslate) * 2.0 - 1.0);//Convert from range [0, 1] to [-1, 1]
    vec4 normal1 = normalize(TriplanarSample(textures[materialInstance.Texture5], tpmBlend, material1ScaleTranslate) * 2.0 - 1.0);
    vec4 normal2 = normalize(TriplanarSample(textures[materialInstance.Texture7], tpmBlend, material2ScaleTranslate) * 2.0 - 1.0);
    vec4 normal3 = normalize(TriplanarSample(textures[materialInstance.Texture9], tpmBlend, material3ScaleTranslate) * 2.0 - 1.0);

    //Convert normal maps to object space
    vec3 tangentPrime = vec3(1.0f, 0.0f, 0.0f);
    vec3 bitangent = cross(fragNormal.xyz, tangentPrime);
    vec3 tangent = cross(bitangent, fragNormal.xyz);
    mat3 texSpace = mat3(tangent, bitangent, fragNormal.xyz);
    normal0 = NormalToObjectSpace(normal0.xyz, texSpace);
    normal1 = NormalToObjectSpace(normal1.xyz, texSpace);
    normal2 = NormalToObjectSpace(normal2.xyz, texSpace);
    normal3 = NormalToObjectSpace(normal3.xyz, texSpace);

    //Calculate final normal
    vec4 finalNormal = vec4(fragNormal.xyz, 1.0f) + normal0;
    finalNormal += normal3 * blendWeights.w;
    finalNormal += normal1 * blendWeights.z;
    finalNormal += normal2 * blendWeights.y;
    finalNormal += normal3 * blendWeights.x;
    finalNormal = normalize(finalNormal);

    return finalNormal;
}

vec4 NormalToObjectSpace(vec3 normal, mat3 tangentSpace)
{
    //return vec4(normalize(mul(normal, tangentSpace)), 1.0f);
    return vec4(normalize(tangentSpace * normal), 1.0f);
}

vec4 TriplanarSample(sampler2D tex, vec3 blend, vec4 uvScaleTranslate)
{
    //Sample texture from 3 directions & blend the results
    vec4 xaxis = texture(tex, fragWorldPos.zy * uvScaleTranslate.xy + uvScaleTranslate.zw);
    vec4 yaxis = texture(tex, fragWorldPos.xz * uvScaleTranslate.xy + uvScaleTranslate.zw);
    vec4 zaxis = texture(tex, fragWorldPos.yx * uvScaleTranslate.xy + uvScaleTranslate.zw);
    vec4 finalTex = xaxis * blend.x + yaxis * blend.y + zaxis * blend.z;
    return finalTex;
}