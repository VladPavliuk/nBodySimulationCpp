struct GSInput
{
	float4 inPosition : SV_POSITION;
	float4 inColor : COLOR;
};

struct GSOutput
{
	float4 outPosition : SV_POSITION;
	float4 outColor : COLOR;
};

[maxvertexcount(3)]
void main(
	point GSInput input[1] : SV_POSITION,
	inout TriangleStream<GSOutput> outputStream
)
{
	GSOutput element;
	//> front triangle
	// left bottom
	element.outPosition = input[0].inPosition;
	element.outPosition.x -= 0.1f;
	element.outPosition.y -= 0.1f;
	//element.outPosition.z -= 0.1f;
	element.outColor = input[0].inColor;
	outputStream.Append(element);

	// center top
	element.outPosition = input[0].inPosition;
	element.outPosition.y += 0.1f;
	element.outColor = input[0].inColor;
	outputStream.Append(element);

	// right bottom
	element.outPosition = input[0].inPosition;
	element.outPosition.x += 0.1f;
	element.outPosition.y -= 0.1f;
	//element.outPosition.z -= 0.1f;
	element.outColor = input[0].inColor;
	outputStream.Append(element);

	//<

	//> left back triangle
	// left bottom
	/*element.outPosition = input[0].inPosition;
	element.outPosition.x -= 2+0.1f;
	element.outPosition.y -= 2+0.1f;
	element.outColor = input[0].inColor;
	outputStream.Append(element);

	// center top
	element.outPosition = input[0].inPosition;
	element.outPosition.y += 2+0.1f;
	element.outColor = input[0].inColor;
	outputStream.Append(element);

	// right bottom
	element.outPosition = input[0].inPosition;
	element.outPosition.x += 2+0.1f;
	element.outPosition.y -= 2+0.1f;
	element.outColor = input[0].inColor;
	outputStream.Append(element);
	*/
	outputStream.RestartStrip();
}