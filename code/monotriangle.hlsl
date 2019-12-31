#if VERTEX_HLSL

struct VS_INPUT
{
    float3 Pos : POSITION;
};

float4
main(VS_INPUT Input) : SV_POSITION
{
    float4 Position = float4(Input.Pos, 1.f);

    return(Position);
}

#elif PIXEL_HLSL

float4
main() : SV_TARGET
{
    float4 Color = float4(0.8f, 0.9f, 0.5f, 1.f);

    return(Color);
}

#endif
