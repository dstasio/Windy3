struct VS_OUTPUT
{
    float4 pos   : SV_POSITION;
    float2 txc   : TEXCOORD;
};

#if VERTEX_HLSL // ---------------------------------------------------
//#pragma pack_matrix(row_major)

struct VS_INPUT
{
    float3 pos  : POSITION;
    float3 norm : NORMAL;
    float2 txc  : TEXCOORD;
};

cbuffer Matrices: register(b0)
{
    float4x4 model;
    float4x4 camera;
    float4x4 projection;
}

VS_OUTPUT
main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4x4 total_matrix = mul(projection, mul(camera, model));
    output.pos = mul(total_matrix,float4(input.pos, 1.f));
    output.txc = input.txc;
    return(output);
}

#elif PIXEL_HLSL // --------------------------------------------------

SamplerState TextureSamplerState
{
    Filter = MIN_MAG_MIP_LINEAR;
};

struct PS_OUTPUT
{
    float4 color : SV_TARGET;
};

Texture2D SampleTexture;

PS_OUTPUT
main(VS_OUTPUT input)
{
    PS_OUTPUT output; 
    output.color = SampleTexture.Sample(TextureSamplerState, input.txc);

    return(output);
}

#endif
