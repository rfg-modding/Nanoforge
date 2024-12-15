
#version 330 core

layout (location = 0) in vec3 vPosition;
layout (location = 1) in vec4 vNormal;
layout (location = 2) in vec4 vTangent;
layout (location = 3) in ivec2 vUv0;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

//out vec2 fUv;
out vec3 WorldPosition;
out vec3 Normal;

void main()
{
    //Multiplying our uniform with the vertex position, the multiplication order here does matter.
    gl_Position = uProjection * uView * uModel * vec4(vPosition, 1.0);
    WorldPosition = vec3(gl_Position.x, gl_Position.y, gl_Position.z);
    
    vec4 normal = vNormal;
    normal = normalize(normal * 2.0f - 1.0f);
    Normal = vec3(normal.x, normal.y, normal.z);
}