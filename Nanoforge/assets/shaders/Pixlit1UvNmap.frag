#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragTangent;
layout(location = 2) in vec4 fragNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    //TODO: Change this to be provided by the per-frame push constant or a per-frame constants buffer
    vec3 sunPos = vec3(1000.0f, 1000.0f, 1000.0f);
    vec3 sunDirection = normalize(vec3(0.0f, 0.0f, 0.0f) - sunPos);

    //TODO: Change this to be provided by the per-frame push constant or a per-frame constants buffer
    vec3 diffuseColor = 1.0f.xxx;
    float diffuseIntensity = 0.7f;
    
    vec4 color = 0.7f.xxxx; //TODO: Change to sample from diffuse textures once available
    vec4 normal = fragNormal; //TODO: Change to also use normal maps in addition to vertex normals when available 
    
    //Ambient light
    float ambientIntensity = 0.05f;
    vec3 ambient = ambientIntensity * color.xyz;
    
    //Diffuse
    vec3 lightDir = -normalize(sunDirection);
    float diff = max(dot(lightDir, normal.xyz), 0.0f);
    vec3 diffuse = diff * color.xyz * diffuseColor * diffuseIntensity;
    
    //TODO: Implement specular highlights
    
    outColor = vec4(ambient + diffuse, 1.0f);
    //outColor = texture(texSampler, fragTexCoord);
    //outColor = fragNormal;
}
