#version 460
#include "Constants.glsl"

layout(early_fragment_tests) in;

layout(binding = 1) uniform sampler2D diffuseSampler;
layout(binding = 2) uniform sampler2D normalSampler;
layout(binding = 3) uniform sampler2D specularSampler;

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec4 fragTangent;
layout(location = 3) in vec4 fragNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    //TODO: Change this to be provided by the per-frame push constant or a per-frame constants buffer
    vec3 sunPos = vec3(1000.0f, 1000.0f, 1000.0f);
    vec3 sunDirection = normalize(vec3(0.0f, 0.0f, 0.0f) - sunPos);
    vec3 sunlightColor = vec3(1.0f, 1.0f, 1.0f);
    float sunlightIntensity = 1.0f;

    vec4 diffuseColor = texture(diffuseSampler, fragTexCoord);

    //TODO: Change to also use normal maps in addition to vertex normals when available. First need to fix or regenerate vertex tangents
    vec3 normal = fragNormal.xyz;

    //Diffuse
    float ambientIntensity = 0.07f; //Control the minimum darkness so things faces that aren't in the direction of the sun are still visible
    vec3 lightDir = -normalize(sunDirection);
    float amountLit = max(dot(lightDir, normal.xyz), ambientIntensity);
    vec3 diffuse = amountLit * diffuseColor.xyz * sunlightColor * sunlightIntensity;

    //Specular highlights
    vec3 viewDir = normalize(ubo.cameraPos.xyz - fragWorldPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    vec3 reflectDir = reflect(-lightDir, normal.xyz);
    float highlightSize = 64.0f; //Smaller = bigger specular highlights
    //float spec = pow(max(dot(viewDir, reflectDir), 0.0f), highlightSize);
    float spec = pow(max(dot(normal.xyz, halfwayDir), 0.0f), highlightSize);
    vec4 specularStrength = texture(specularSampler, fragTexCoord);
    vec3 specular = spec * specularStrength.xyz;

    outColor = vec4(diffuse + specular, 1.0f);
}
