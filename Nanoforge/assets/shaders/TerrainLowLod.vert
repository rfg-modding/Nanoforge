#version 460
#include "Constants.glsl"

layout(location = 0) in ivec4 inPosition;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 outZonePos;

void main()
{
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
    terrainRange *= 0.9980; //Hack to fill in gaps between zones. May cause object positions to be slightly off
    float zoneRange = zoneMax - zoneMin;

    //Scale position
    vec4 posFloat = inPosition.xyzw;
    posFloat.x = (((posFloat.x - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.y = (((posFloat.y - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.z = (((posFloat.z - terrainMin) * zoneRange) / terrainRange) + zoneMin;
    posFloat.w = 1.0f;
    posFloat.y *= 2.0f;
    outZonePos = posFloat.xyz;
    gl_Position = ubo.proj * ubo.view * objectBuffer.objects[gl_BaseInstance].model * posFloat;

    //Calc texture UV
    fragTexCoord.x = ((posFloat.x + 255.5f) / 511.0f);
    fragTexCoord.y = ((posFloat.z + 255.5f) / 511.0f);
    fragTexCoord.x *= 0.9980; //Part of hack to fix terrain gaps. Must scale UVs since we're scaling the terrain size
    fragTexCoord.y *= 0.9980;
}
