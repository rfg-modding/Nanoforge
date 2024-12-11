#include "Constants.hlsl"

#define kAntialiasing 2.0f

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float Size : SIZE;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
    float Size : SIZE;
    float EdgeDistance : EDGE_DISTANCE;
};

VS_OUTPUT VS(float4 inPosAndSize : POSITION, float4 inColor : COLOR)
{
    VS_OUTPUT output;
    output.Pos = mul(float4(inPosAndSize.xyz, 1.0f), WVP);
    output.Size = max(inPosAndSize.w, kAntialiasing);
    output.Color = float4(inColor.xyz, 1.0f);

    return output;
}

//Note: This geometry shader is based off this one from im3d: https://github.com/john-chapman/im3d/blob/0bcc8b892becdd11c49e8cbfc2fb321e0879eeec/examples/DirectX11/im3d.hlsl#L45
//Geometry shader. Converts each line to a quad in order to create thick lines
[maxvertexcount(4)] //Creating a quad, so 4 vertices
void GS(line VS_OUTPUT vertices[2] : SV_POSITION, inout TriangleStream<PS_INPUT> outputStream)
{
    VS_OUTPUT vert0 = vertices[0];
    VS_OUTPUT vert1 = vertices[1];

    float2 pos0 = vert0.Pos.xy / vert0.Pos.w;
	float2 pos1 = vert1.Pos.xy / vert1.Pos.w;
	
	float2 dir = pos0 - pos1;
	dir = normalize(float2(dir.x, dir.y * ViewportDimensions.y / ViewportDimensions.x)); //Correct for aspect ratio
	float2 tng0 = float2(-dir.y, dir.x);
	float2 tng1 = tng0 * vert1.Size / ViewportDimensions;
	tng0 = tng0 * vert0.Size / ViewportDimensions;
	
	PS_INPUT output;
			
	//Line start
    output.Size = vert0.Size;
	output.Color = vert0.Color;
	output.Pos = float4((pos0 - tng0) * vert0.Pos.w, vert0.Pos.zw); 
    output.EdgeDistance = -vert0.Size;
	outputStream.Append(output);
	output.Pos = float4((pos0 + tng0) * vert0.Pos.w, vert0.Pos.zw);
    output.EdgeDistance = vert0.Size;
	outputStream.Append(output);
			
	//Line end
    output.Size = vert1.Size;
	output.Color = vert1.Color;
	output.Pos = float4((pos1 - tng1) * vert1.Pos.w, vert1.Pos.zw);
    output.EdgeDistance = -vert1.Size;
	outputStream.Append(output);
	output.Pos = float4((pos1 + tng1) * vert1.Pos.w, vert1.Pos.zw);
    output.EdgeDistance = vert1.Size;
	outputStream.Append(output);
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 ret = input.Color;

    //Code from im3d that can be used to anti alias the lines
    float d = abs(input.EdgeDistance) / input.Size;
 	d = smoothstep(1.0, 1.0 - (kAntialiasing / input.Size), d);
    
    ret.a *= d;
    return ret;
}