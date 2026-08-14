#pragma once

#include "ConstantBuffers.h"
#include "D3D11Utils.h"
#include "Mesh.h"
#include "MeshData.h"
#include "PhysicsBody.h"

namespace hlab {

using DirectX::SimpleMath::Matrix;

// 인스턴스별로 넘기는 데이터는 world 행렬 하나뿐이다.
// 물리 구는 항상 균일 스케일(반지름)만 쓰므로 world의 3x3 부분을 그대로 normal 변환에도
// 재사용할 수 있어(BasicInstancedVS.hlsl 참고) worldIT를 따로 계산/전송할 필요가 없다.
// cbuffer와 달리 버텍스버퍼는 column_major 패킹이 적용되지 않으므로 CPU에서 transpose 하지 않는다.
struct SphereInstanceData {
    Matrix world;
};

// GPU 하드웨어 인스턴싱으로 물리 구 여러 개를 드로우콜 1번에 그리는 전용 클래스.
// 반지름 1인 구 메시 하나 + mainSphere와 동일한 돌 PBR 텍스처를 모든 인스턴스가 공유하고,
// 실제 반지름/위치/회전은 인스턴스별 world 행렬로만 표현한다.
class InstancedSpheres {
  public:
    void Initialize(ComPtr<ID3D11Device> &device, ComPtr<ID3D11DeviceContext> &context,
                    int maxInstanceCount);

    // 매 프레임 physics body들의 현재 상태로 인스턴스 버퍼를 갱신
    void UpdateInstances(ComPtr<ID3D11DeviceContext> &context, const PhysicsBody bodies[],
                         int count);

    void Render(ComPtr<ID3D11DeviceContext> &context);

  private:
    shared_ptr<Mesh> m_mesh;

    // useHeightMap/heightScale만 실제로 사용됨
    MeshConstants m_meshConstsCPU;
    // mainSphere와 동일한 설정
    MaterialConstants m_materialConstsCPU;

    ComPtr<ID3D11Buffer> m_meshConstsGPU;
    ComPtr<ID3D11Buffer> m_materialConstsGPU;
    ComPtr<ID3D11Buffer> m_instanceBuffer;

    int m_maxInstanceCount = 0;
    int m_instanceCount = 0;
};

} // namespace hlab
