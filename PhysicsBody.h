#pragma once

#include <directxtk/SimpleMath.h>

namespace hlab {

using DirectX::SimpleMath::Vector3;

// 1단계: 구 하나 + 중력 + 바닥 충돌(반발)만 다루는 최소 단위 물리 바디
class PhysicsBody {
  public:
    Vector3 position;
    Vector3 velocity; // 속도
    float radius = 0.2f;
    float restitution = 0.8f; // 반발계수 (1.0 = 완전탄성, 0.0 = 완전비탄성)

    void Update(float dt, float floorHeight);

    // 2단계: 구-구 충돌 (겹침 보정 + 반발 임펄스). a, b 둘 다 직접 수정됨.
    static void ResolveCollision(PhysicsBody &a, PhysicsBody &b);
};

} // namespace hlab
