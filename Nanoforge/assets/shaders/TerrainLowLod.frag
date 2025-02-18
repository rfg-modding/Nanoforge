#version 460

layout(binding = 1) uniform sampler2D texSampler;
layout(binding = 2) uniform sampler2D texSampler2;
layout(binding = 3) uniform sampler2D texSampler3;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 inZonePos;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(texSampler, fragTexCoord);

    //Get terrain color from _comb texture and adjust it's brightness
    //float4 blendValues = Texture0.Sample(Sampler0, input.Uv);
    //float gamma = 0.5f;
    //float3 terrainColor = pow(blendValues.xyz, 1.0 / gamma);
    
    //Get pre-calculated lighting from _ovl texture
    //float3 lighting = Texture1.Sample(Sampler1, input.Uv).xyz;;
    
    //Ambient
    //float ambientIntensity = 0.15f;
    //float3 ambient = ambientIntensity * terrainColor;
    
    //Diffuse
    //float3 diffuse = lighting * DiffuseColor * DiffuseIntensity * terrainColor;
    //diffuse *= 1.5f; //Arbitrary scaling to look similar to high lod terrain. Temporary fix until high lod terrain is fully working
    
    //Normalized elevation for ShadeMode 0. [-255.5, 255.5] to [0.0, 511.0] to [0.0, 1.0]
    float elevationNormalized = (inZonePos.y + 255.5f) / 511.0f;
    
    //TODO: Add ShadeMode to constants buffer and make it available to fragment shaders
    int ShadeMode = 1;
    if (ShadeMode == 0)
    {
        //Color terrain only by elevation
        outColor = vec4(elevationNormalized, elevationNormalized, elevationNormalized, 1.0f);
    }
    else if (ShadeMode == 1)
    {
        //TODO: Uncomment once ambient code is fixed
        //Color terrain with basic lighting
        //outColor =  vec4(ambient + diffuse, 1.0f);
        //outColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
        
        vec4 blendValues = texture(texSampler, fragTexCoord);
        float gamma = 0.2f;
        vec4 terrainColor = vec4(pow(blendValues.x, 1.0f / gamma), pow(blendValues.y, 1.0f / gamma), pow(blendValues.z, 1.0f / gamma), 1.0f);
        vec4 lighting = texture(texSampler2, fragTexCoord);
        outColor = terrainColor * lighting;
    }
    else
    {
        //If given invalid ShadeMode use elevation coloring (mode 0)
        outColor =  vec4(elevationNormalized, elevationNormalized, elevationNormalized, 1.0f);
    }
}
