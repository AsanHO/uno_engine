#pragma once

#include <directxtk/SimpleMath.h>
#include <vector>

namespace hlab {

using DirectX::SimpleMath::Vector2;
using DirectX::SimpleMath::Vector3;

struct Vertex {
    Vector3 position;
    Vector3 normalModel;
    Vector2 texcoord;
    Vector3 tangentModel;
    Vector3 color; // 임시로 넣음 todo:: 삭제 및 수정 필요
    // Vector3 biTangentModel;
    //  biTangent는 쉐이더에서 계산
};

} // namespace hlab