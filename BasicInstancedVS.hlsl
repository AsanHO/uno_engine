#include "Common.hlsli" // 셰이더끼리 include 헤더 공유

// Vertex Shader에서만 쓰는 텍스처
Texture2D g_heightTexture : register(t0);

// InstancedSpheres가 기존 MeshConstants 구조체를 그대로 올리는 cbuffer.
// world/worldIT 필드는 인스턴싱 경로에서는 안 쓰고(각 정점의 world는 아래 인스턴스 입력에서 옴),
// useHeightMap/heightScale만 사용한다.
cbuffer BasicVertexConstantData : register(b0)
{
    matrix unusedWorld;
    matrix unusedWorldIT;
    int useHeightMap;
    float heightScale;
    float2 dummy;
};

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

PixelShaderInput main(InstancedVertexShaderInput input)
{
    PixelShaderInput output;

    // 정점버퍼로 넘어온 행렬은 cbuffer의 column_major 패킹 트릭이 적용되지 않으므로,
    // CPU에서 transpose 하지 않은 row-major 행렬을 그대로 4개 행으로 재구성한다.
    matrix world = float4x4(input.world0, input.world1, input.world2, input.world3);

    // 물리 구는 항상 균일 스케일(반지름)만 쓰므로 world의 3x3 부분을 그대로 normal 변환에 써도
    // normalize()가 양의 배율을 지워버려서 worldIT를 따로 계산/전송한 것과 결과가 같다.
    float4 normal = float4(input.normalModel, 0.0f);
    output.normalWorld = normalize(mul(normal, world).xyz);

    float4 tangentWorld = mul(float4(input.tangentModel, 0.0f), world);

    float4 pos = float4(input.posModel, 1.0f);
    pos = mul(pos, world);

    if (useHeightMap)
    {
        // VertexShader에서는 SampleLevel 사용
        float height = g_heightTexture.SampleLevel(linearClampSampler, input.texcoord, 0).r;
        height = height * 2.0 - 1.0;
        pos += float4(output.normalWorld * height * heightScale, 0.0);
    }

    output.posWorld = pos.xyz;

    pos = mul(pos, viewProj);

    output.posProj = pos;
    output.texcoord = input.texcoord;
    output.tangentWorld = tangentWorld.xyz;

    return output;
}
