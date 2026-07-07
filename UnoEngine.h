#pragma once

#include <algorithm>
#include <directxtk/SimpleMath.h>
#include <iostream>
#include <memory>
#include <vector>

#include "BasicMeshGroup.h"
#include "CubeMapping.h"
#include "EngineBase.h"

namespace hlab {

using DirectX::SimpleMath::Matrix;
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
    BasicMeshGroup m_mainSphere;
    BasicMeshGroup m_mainSphere2;
    CubeMapping m_cubeMapping;

    bool m_usePerspectiveProjection = true;
};
} // namespace hlab