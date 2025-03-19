#version 460
#include "Constants.glsl"

layout(location = 0) in vec3 vertexZonePos;
layout(location = 1) in vec3 vertexWorldPos;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragNormal;
layout(location = 4) flat in int fragObjectIndex;

layout(location = 0) out vec4 outColor;

//TODO: Put the color space conversion functions in a different file once #include is supported in shaders
#ifndef saturate
#define saturate(v) clamp(v, 0., 1.)
#endif
vec3 hue2rgb(float hue){
    hue=fract(hue);
    return saturate(vec3(
    abs(hue*6.-3.)-1.,
    2.-abs(hue*6.-2.),
    2.-abs(hue*6.-4.)
    ));
}

//RGB to HSL (hue, saturation, lightness/luminance).
//Source: https://gist.github.com/yiwenl/745bfea7f04c456e0101
vec3 rgb2hsl(vec3 c){
    float cMin=min(min(c.r, c.g), c.b),
    cMax=max(max(c.r, c.g), c.b),
    delta=cMax-cMin;
    vec3 hsl=vec3(0., 0., (cMax+cMin)/2.);
    if (delta!=0.0){ //If it has chroma and isn't gray.
        if (hsl.z<.5){
            hsl.y=delta/(cMax+cMin);//Saturation.
        } else {
            hsl.y=delta/(2.-cMax-cMin);//Saturation.
        }
        float deltaR=(((cMax-c.r)/6.)+(delta/2.))/delta,
        deltaG=(((cMax-c.g)/6.)+(delta/2.))/delta,
        deltaB=(((cMax-c.b)/6.)+(delta/2.))/delta;
        //Hue.
        if (c.r==cMax){
            hsl.x=deltaB-deltaG;
        } else if (c.g==cMax){
            hsl.x=(1./3.)+deltaR-deltaB;
        } else { //if(c.b==cMax){
            hsl.x=(2./3.)+deltaG-deltaR;
        }
        hsl.x=fract(hsl.x);
    }
    return hsl;
}

//HSL to RGB.
//Source: https://github.com/Jam3/glsl-hsl2rgb/blob/master/index.glsl
vec3 hsl2rgb(vec3 hsl){
    if (hsl.y==0.){
        return vec3(hsl.z);//Luminance.
    } else {
        float b;
        if (hsl.z<.5){
            b=hsl.z*(1.+hsl.y);
        } else {
            b=hsl.z+hsl.y-hsl.y*hsl.z;
        }
        float a=2.*hsl.z-b;
        return a+hue2rgb(hsl.x)*(b-a);
    }
}

vec4 NormalToObjectSpace(vec3 normal, mat3 tangentSpace)
{
    //return vec4(normalize(mul(normal, tangentSpace)), 1.0f);
    return vec4(normalize(tangentSpace * normal), 1.0f);
}

vec4 TriplanarSample(sampler2D tex, vec3 blend, vec4 uvScaleTranslate)
{
    //Sample texture from 3 directions & blend the results
    vec4 xaxis = texture(tex, vertexWorldPos.zy * uvScaleTranslate.xy + uvScaleTranslate.zw);
    vec4 yaxis = texture(tex, vertexWorldPos.xz * uvScaleTranslate.xy + uvScaleTranslate.zw);
    vec4 zaxis = texture(tex, vertexWorldPos.yx * uvScaleTranslate.xy + uvScaleTranslate.zw);
    vec4 finalTex = xaxis * blend.x + yaxis * blend.y + zaxis * blend.z;
    return finalTex;
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

void main()
{
    vec3 sunPos = vec3(1000.0f, 1000.0f, 1000.0f);//TODO: Define this in per-frame UBO
    vec3 sunDirection = normalize(vec3(0.0f, 0.0f, 0.0f) - sunPos);
    vec3 sunlightColor = vec3(1.0f, 1.0f, 1.0f);
    float sunlightIntensity = 1.0f;
    MaterialInstance materialInstance = materialBuffer.materials[objectBuffer.objects[fragObjectIndex].MaterialIndex];

    //Get weight of each texture from alpha00 texture and make sure they sum to 1.0
    vec4 blendWeightsBase = texture(textures[materialInstance.Texture0], fragTexCoord);
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

    outColor = vec4(diffuse, 1.0f);
}