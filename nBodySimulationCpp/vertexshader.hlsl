cbuffer testConstantBuffer : register(b0)
{
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
};

struct VS_INPUT
{
    float3 inPos : POSITION;
    float3 inColor : COLOR;
    //float2 inTexCoord : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 outPosition : SV_POSITION;
    float4 outColor : COLOR;
    //float2 outTexCoord : COLOR;
};

VS_OUTPUT main(VS_INPUT input, uint id: SV_VertexID)
{
    VS_OUTPUT output;
    
    output.outPosition = mul(float4(input.inPos, 1.0f), mul(viewMatrix, projectionMatrix));
    //output.outPosition = mul(float4(input.inPos, 1.0f), viewMatrix);
    //output.outPosition = float4(input.inPos, 1.0f);
    output.outColor = float4(input.inColor.x, input.inColor.y, input.inColor.z, 1.0f);

    return output;
}