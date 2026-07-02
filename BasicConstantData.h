#pragma once

#include <directxtk/SimpleMath.h>

#include "Light.h"
#include "Material.h"

namespace hlab {

using DirectX::SimpleMath::Matrix;
using DirectX::SimpleMath::Vector2;
using DirectX::SimpleMath::Vector3;

struct BasicVertexConstantData {
    Matrix modelWorld;
    Matrix invTranspose;
    Matrix view;
    Matrix projection;
    int useHeightMap = 1;
    float heightScale = 0.0f;
    Vector2 dummy;
};

static_assert((sizeof(BasicVertexConstantData) % 16) == 0,
              "Constant Buffer size must be 16-byte aligned");

struct BasicPixelConstantData {
    Vector3 eyeWorld;          // 12
    float mipmapLevel = 0.0f;  // 4
    Material material;         // 48
    Light lights[MAX_LIGHTS];  // 48 * MAX_LIGHTS
    Vector4 indexColor;        // 피킹(Picking)에 사용
    int useAlbedoMap = 0;      // 4
    int useNormalMap = 0;      // 4
    int useAOMap = 0;          // 4, Ambient Occlusion
    int invertNormalMapY = 0;  // 4
    int useMetallicMap = 0;
    int useRoughnessMap = 0;
    float expose = 1.0f;
    float gamma = 1.0f;
};

static_assert((sizeof(BasicPixelConstantData) % 16) == 0,
              "Constant Buffer size must be 16-byte aligned");

struct NormalVertexConstantData {
    float scale = 0.1f;
    float dummy[3];
};

} // namespace hlab
