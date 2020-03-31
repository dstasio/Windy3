struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float2 TXC : TEXCOORD;
};

#if VERTEX_HLSL // ---------------------------------------------------

struct VS_INPUT
{
    float3 Pos : POSITION;
    float2 TXC : TEXCOORD;
};

cbuffer Matrices: register(b0)
{
    float4x4 Model;
    float4x4 Camera;
    float4x4 Projection;
}

VS_OUTPUT
main(VS_INPUT Input)
{
    VS_OUTPUT Output;
    float4x4 total_matrix = mul(Projection, mul(Camera, Model));
    Output.Pos = mul(total_matrix,float4(Input.Pos, 1.f));
    Output.TXC = Input.TXC;
    return(Output);
}

#elif PIXEL_HLSL // --------------------------------------------------

SamplerState TextureSamplerState
{
    Filter = MIN_MAG_MIP_LINEAR;
};

Texture2D SampleTexture;

float4
main(VS_OUTPUT Input) : SV_TARGET
{
    float4 Color = SampleTexture.Sample(TextureSamplerState, Input.TXC);

    return(Color);
}

#endif
