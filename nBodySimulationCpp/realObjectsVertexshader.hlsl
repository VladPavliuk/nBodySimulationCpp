cbuffer testConstantBuffer : register(b0)
{
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
};

struct VS_INPUT
{
    float3 inPos : POSITION;
    float2 inTexCoord : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 outPosition : SV_POSITION;
    float2 outTexCoord : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    output.outPosition = mul(float4(input.inPos, 1.0f), mul(viewMatrix, projectionMatrix));
    //output.outPosition = float4(input.inPos, 1.0f);
    output.outTexCoord = input.inTexCoord;

    return output;
}