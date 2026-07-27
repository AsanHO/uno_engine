#pragma once
#include <DirectXCollision.h>
#include <algorithm>
#include <directxtk/SimpleMath.h>
#include <iostream>
#include <memory>
#include <vector>

#include "EngineBase.h"
#include "Model.h"

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

  protected:
    shared_ptr<Model> m_mainSphere;
    shared_ptr<Model> m_skybox;
    Light m_light;
    shared_ptr<Model> m_lightSphere;

    bool m_usePerspectiveProjection = true;

   
    // 유저 인터랙션
    BoundingSphere m_mainBoundingSphere;
    shared_ptr<Model> m_cursorSphere;

    // 거울
    shared_ptr<Model> m_mirror;
    DirectX::SimpleMath::Plane m_mirrorPlane;
    float m_mirrorAlpha = 0.5f;

    // 거울 제외 물체 리스트
    vector<shared_ptr<Model>> m_basicList;
};
} // namespace hlab