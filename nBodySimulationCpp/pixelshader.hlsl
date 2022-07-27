struct PS_INPUT
{
	float4 inPosition : SV_POSITION;
	float4 inColor : COLOR;
	//float2 inTexCoord : TEXCOORD;
};

Texture2D objTexture : TEXTURE: register(t0);
SamplerState objSamplerState : SAMPLER: register(s0);

float4 main(PS_INPUT input) : SV_TARGET
{
	//float3 pixelColor = objTexture.Sample(objSamplerState, input.inTexCoord.xy).xyz;

	//return float4(pixelColor, 1.0f);
	//return float4(1.0f, 1.0f, 0.0f, 1.0f);
	return input.inColor;
}