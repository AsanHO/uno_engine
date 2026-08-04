#include "PhysicsBody.h"

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
    }
}

} // namespace hlab
