StructuredBuffer<float3> inputFakeObjectsBuffer : register(t0);
StructuredBuffer<float3> inputRealObjectsBuffer : register(t1);
RWStructuredBuffer<float3> ouputBuffer : register(u0);

float3 getForce(float3 objectA, float3 objectB)
{
	float3 dV = objectA - objectB;

	//float distance = length(dV);
    
	//float force = 1.0f / (distance * distance + 1.0f);

	//return dV * force;
    return dV / max(dot(dV, dV), 0.00001);

}

[numthreads(32, 1, 1)]
void main(uint3 dispatchThreadId: SV_DispatchThreadID)
{
    //uint fakeObjectIndex = dispatchThreadId.x + dispatchThreadId.y * 32 * 128;
	
   // for (uint i = 0; i < 100; i++)
    //{
    float3 force = float3(0.0f, 0.0f, 0.0f);
    
    uint fakeObjectIndex = dispatchThreadId.x + dispatchThreadId.y * 40960;
	
    for (uint j = 0; j < 1; j++)
    {
        force += getForce(inputRealObjectsBuffer[j], inputFakeObjectsBuffer[fakeObjectIndex]);
    }
        
    ouputBuffer[fakeObjectIndex] = force;
    //}
}