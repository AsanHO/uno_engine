#include "PhysicsBody.h"

#include <algorithm>

namespace hlab {

void PhysicsBody::Update(float dt, float floorHeight) {
    const Vector3 gravity(0.0f, -9.8f, 0.0f);
    
    // 중력 적분 (Explicit Euler)
    //position += velocity * dt;
    //velocity += gravity * dt;

    // 중력 적분 (semi-implicit Euler)
    velocity += gravity * dt;
    position += velocity * dt;

    // 바닥 충돌: 중심의 y좌표 - 반지름 < 바닥 높이
    if (position.y - radius < floorHeight) {
        position.y = floorHeight + radius;

        if (velocity.y < 0.0f) {
            velocity.y = -velocity.y * restitution;
        }

        // 마찰: 바닥에 닿아있는 동안 수평(x, z) 속도를 감쇠 : max는 음수 방지 . 음수가 나올일은
        // 없지만 추후 dt가 커지면 음수가 나올 수 있음
        float frictionFactor = (std::max)(0.0f, 1.0f - friction * dt);
        velocity.x *= frictionFactor;
        velocity.z *= frictionFactor;
    }

    // 구르는 회전: 수평 이동 방향에 수직인 축(up × moveDir)으로, (속력/반지름)만큼 회전
    Vector3 horizontalVelocity(velocity.x, 0.0f, velocity.z);
    float speed = horizontalVelocity.Length();
    if (speed > 1e-4f) {
        Vector3 moveDir = horizontalVelocity / speed;
        const Vector3 up(0.0f, 1.0f, 0.0f);
        Vector3 axis = up.Cross(moveDir); // moveDir이 수평이라 이미 단위벡터
        float angle = (speed / radius) * dt;

        orientation = orientation * Quaternion::CreateFromAxisAngle(axis, angle);
        orientation.Normalize(); // 누적 곱셈으로 인한 부동소수점 오차 방지
    }
}

void PhysicsBody::ResolveCollision(PhysicsBody &a, PhysicsBody &b) {
    Vector3 delta = b.position - a.position;
    float distance = delta.Length();
    float radiusSum = a.radius + b.radius;

    if (distance >= radiusSum || distance < 1e-6f) {
        return; // 안 겹쳤거나, 중심이 완전히 겹친 예외 상황
    }

    Vector3 normal = delta / distance; // A -> B 방향 단위벡터(n hat)

    // 1) 위치 보정: 겹친 만큼 절반씩 반대 방향으로 밀어냄
    float overlap = radiusSum - distance;
    a.position -= normal * (overlap * 0.5f);
    b.position += normal * (overlap * 0.5f);

    // 2) 속도 반응: 다가오는 성분(내적)만 반발계수만큼 남기고 뒤집음
    Vector3 relativeVelocity = b.velocity - a.velocity;
    float speedAlongNormal = relativeVelocity.Dot(normal);

    if (speedAlongNormal > 0.0f) {
        return; // 이미 서로 멀어지는 중
    }

    float restitution = (std::min)(a.restitution, b.restitution);
    float impulse = -(1.0f + restitution) * speedAlongNormal * 0.5f;

    a.velocity -= impulse * normal;
    b.velocity += impulse * normal;
}

} // namespace hlab
