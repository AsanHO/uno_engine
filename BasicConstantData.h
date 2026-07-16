#pragma once

#include <directxtk/SimpleMath.h>

#include "Light.h"
#include "Material.h"

namespace hlab {

using DirectX::SimpleMath::Matrix;
using DirectX::SimpleMath::Vector2;
using DirectX::SimpleMath::Vector3;

struct EyeViewProjConstData {
    // View와 Proj를 미리 곱한 형태
    Matrix viewProj;
    Vector3 eyeWorld; // Eye도 분리 가능
    float dummy;
};

struct BasicVertexConstantData {
    Matrix modelWorld;
    Matrix invTranspose;
    int useHeightMap = 1;
    float heightScale = 0.0f;
    Vector2 dummy;
};

static_assert((sizeof(BasicVertexConstantData) % 16) == 0,
              "Constant Buffer size must be 16-byte aligned");

struct BasicPixelConstantData {
    Material material;        // 48
    Light lights[MAX_LIGHTS]; // 48 * MAX_LIGHTS

    // 여러 옵션들에 uint 플래그 하나만 사용할 수도 있습니다.
    int useAlbedoMap = 0;
    int useNormalMap = 0;
    int useAOMap = 0;         // Ambient Occlusion
    int invertNormalMapY = 0; // 16
    int useMetallicMap = 0;
    int useRoughnessMap = 0;
    int useEmissiveMap = 0;
    float expose = 1.0f; // 16
    float gamma = 1.0f;
    float mipmapLevel = 0.0f;
    Vector2 dummy; // 16
};

static_assert((sizeof(BasicPixelConstantData) % 16) == 0,
              "Constant Buffer size must be 16-byte aligned");

struct NormalVertexConstantData {
    float scale = 0.1f;
    float dummy[3];
};

} // namespace hlab
