

float4 VS(float4 Pos : POSITION) : SV_POSITION
{
    return Pos;
}

float4 PS(float4 Pos : SV_POSITION) : SV_TARGET
{
    return float4(1.0f, 0.0f, 0.7f, 1.0f);
}