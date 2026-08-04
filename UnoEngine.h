#pragma once
#include <algorithm>
#include <directxtk/SimpleMath.h>
#include <iostream>
#include <memory>

#include "GeometryGenerator.h"
#include "ImageFilter.h"
#include "EngineBase.h"
#include "Model.h"
#include "PhysicsBody.h"

namespace hlab {

using DirectX::BoundingSphere;
using DirectX::SimpleMath::Vector3;

class UnoEngine : public EngineBase {
  public:
    UnoEngine();
    virtual bool Initialize() override;
    virtual void UpdateGUI() override;
    virtual void Update(float dt) override;
    virtual void Render() override;
    void UpdateLights(float dt);
  protected:
    shared_ptr<Model> m_mainSphere;
    shared_ptr<Model> m_skybox;
    shared_ptr<Model> m_ground;
    shared_ptr<Model> m_lightSphere[MAX_LIGHTS];

    bool m_usePerspectiveProjection = true;

   
    // 유저 인터랙션
    BoundingSphere m_mainBoundingSphere;
    PhysicsBody m_mainPhysicsBody; // mainSphere 물리 (드래그 중엔 정지, 놓으면 중력/바닥충돌 적용)
    shared_ptr<Model> m_cursorSphere;

    // 거울
    shared_ptr<Model> m_mirror;
    DirectX::SimpleMath::Plane m_mirrorPlane;
    float m_mirrorAlpha = 0.5f;

    // 거울 제외 물체 리스트
    vector<shared_ptr<Model>> m_basicList;

    // 물리 데모 (1단계: 구 1개 + 중력 + 바닥 충돌)
    shared_ptr<Model> m_physicsSphere;
    PhysicsBody m_physicsBody;
    float m_floorHeight = -0.5f;
};
} // namespace hlab