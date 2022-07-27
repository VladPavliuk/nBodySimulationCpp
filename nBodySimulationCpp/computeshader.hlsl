StructuredBuffer<float3> inputFakeObjectsBuffer : register(t0);
StructuredBuffer<float3> inputRealObjectsBuffer : register(t1);
RWStructuredBuffer<float3> ouputBuffer : register(u0);

float3 getForce(float3 objectA, float3 objectB)
{
	float3 dV = objectA - objectB;

	float distance = length(dV);

	float force = 1.0f / (distance * distance + 1.0f);

	return dV * force;
}

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadId: SV_DispatchThreadID)
{
	uint fakeObjectIndex = dispatchThreadId.x + dispatchThreadId.y * 64 * 400;
	//uint fakeObjectIndex = dispatchThreadId.x;
	
	float3 force = float3(0.0f, 0.0f, 0.0f);

	for (int i = 0; i < 1; i++)
	{
		force += getForce(inputRealObjectsBuffer[i], inputFakeObjectsBuffer[fakeObjectIndex]);
	}

	ouputBuffer[fakeObjectIndex] = force;
}