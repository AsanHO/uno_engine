#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <directxtk/SimpleMath.h>
#include <memory>

#include "Mesh.h"

namespace hlab {

using DirectX::SimpleMath::Matrix;
using DirectX::SimpleMath::Vector3;

struct Material {
    Vector3 albedo = Vector3(1.0f); // 12
    float roughness = 0.0f;
    float metallic = 0.0f;
    Vector3 emission = Vector3(0.0f);
};

} // namespace hlab