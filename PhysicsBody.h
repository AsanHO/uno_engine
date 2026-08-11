#pragma once

#include <directxtk/SimpleMath.h>

namespace hlab {

using DirectX::SimpleMath::Quaternion;
using DirectX::SimpleMath::Vector3;

// 1단계: 구 하나 + 중력 + 바닥 충돌(반발)만 다루는 최소 단위 물리 바디
class PhysicsBody {
  public:
    Vector3 position;
    Vector3 velocity; // 속도
    float radius = 0.2f;
    float restitution = 0.8f; // 반발계수 (1.0 = 완전탄성, 0.0 = 완전비탄성)
    float friction = 0.6f;    // 마찰계수 (바닥에 닿아있는 동안 수평 속도를 감쇠, 클수록 빨리 멈춤)

    // 구르는 회전 (렌더링 전용 시각 효과, 충돌 계산에는 영향 없음)
    // Quaternion::Identity는 이 프로젝트에서 링크가 안 되어 CreateFromAxisAngle(axis, 0)로 대체
    Quaternion orientation = Quaternion::CreateFromAxisAngle(Vector3(1.0f, 0.0f, 0.0f), 0.0f);

    void Update(float dt, float floorHeight);

    // 2단계: 구-구 충돌 (겹침 보정 + 반발 임펄스). a, b 둘 다 직접 수정됨.
    static void ResolveCollision(PhysicsBody &a, PhysicsBody &b);
};

} // namespace hlab
