#version 330 core
in vec3 WorldPosition;
in vec3 Normal;
//in vec2 fUv;
//
//uniform sampler2D uTexture0;

out vec4 FragColor;

void main()
{
    vec3 color = vec3(1.0f, 1.0f, 1.0f);
    vec3 SunDirection = vec3(0.8f, -0.5f, -1.0f);
    
    vec3 normal = Normal;
    vec3 DiffuseColor = vec3(1.0f, 1.0f, 1.0f);
    float DiffuseIntensity = 1.0f;
    
    vec3 lightDir = -normalize(SunDirection);
    float diff = max(dot(lightDir, normal), 0.0f);
    vec3 diffuse = diff * color.xyz * DiffuseColor * DiffuseIntensity;
    
    FragColor = vec4(diffuse.x, diffuse.y, diffuse.z, 1.0f);
}