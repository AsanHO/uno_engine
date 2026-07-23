#pragma once

#include <algorithm>
#include <directxtk/SimpleMath.h>
#include <iostream>
#include <memory>
#include <vector>
#include <DirectXCollision.h>   // BoundingSphere
#include "BasicMeshGroup.h"
#include "CubeMapping.h"
#include "EngineBase.h"

namespace hlab {

using DirectX::BoundingSphere;
using DirectX::SimpleMath::Vector3;

// 이 예제에서 사용하는 Vertex 정의
// struct Vertex {
//    Vector3 position;
//    Vector3 color;
//};
class UnoEngine : public EngineBase {
  public:
    UnoEngine();

    virtual bool Initialize() override;
    virtual void UpdateGUI() override;
    virtual void Update(float dt) override;
    virtual void Render() override;

  protected:
    shared_ptr<BasicMeshGroup> m_mainSphere;
    shared_ptr<BasicMeshGroup> m_mainSphere2;
    CubeMapping m_cubeMapping;

    Vector3 m_lightPosition = Vector3(0.0f, 1.0f, 0.0f);

    bool m_usePerspectiveProjection = true;

     // 유저 인터랙션 (신규)
    BoundingSphere m_mainBoundingSphere;
    BasicMeshGroup m_cursorSphere;
    

    // 거울
    shared_ptr<BasicMeshGroup> m_mirror;
    DirectX::SimpleMath::Plane m_mirrorPlane;
    float m_mirrorAlpha = 0.5f;

    // 거울 제외 물체 리스트
    vector<shared_ptr<BasicMeshGroup>> m_basicList;
};
} // namespace hlab