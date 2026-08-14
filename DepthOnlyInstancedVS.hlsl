#include "Common.hlsli" // 셰이더끼리 include 헤더 공유

// 정점 데이터(슬롯0) + 인스턴스별 world 행렬(슬롯1, WORLD0~3 네 개의 행)
struct InstancedVertexShaderInput
{
    float3 posModel : POSITION;
    float3 normalModel : NORMAL0;
    float2 texcoord : TEXCOORD0;
    float3 tangentModel : TANGENT0;
    float4 world0 : WORLD0;
    float4 world1 : WORLD1;
    float4 world2 : WORLD2;
    float4 world3 : WORLD3;
};

float4 main(InstancedVertexShaderInput input) : SV_POSITION
{
    // DepthOnlyVS.hlsl과 동일하게 위치만 필요(height map 미사용)
    matrix world = float4x4(input.world0, input.world1, input.world2, input.world3);

    float4 pos = float4(input.posModel, 1.0f);
    pos = mul(pos, world);
    return mul(pos, viewProj);
}
