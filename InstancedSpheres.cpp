#include "InstancedSpheres.h"
#include "GeometryGenerator.h"

#include <algorithm>

namespace hlab {

void InstancedSpheres::Initialize(ComPtr<ID3D11Device> &device,
                                  ComPtr<ID3D11DeviceContext> &context, int maxInstanceCount) {
    m_maxInstanceCount = maxInstanceCount;

    // mainSphere(UnoEngine::Initialize()의 Main Sphere)와 동일한 돌 PBR 텍스처.
    // 반지름은 항상 1로 만들고, 실제 반지름은 인스턴스별 world 행렬의 스케일로 적용한다.
    MeshData sphereMesh = GeometryGenerator::MakeSphere(1.0f, 40, 40, {1.0f, 1.0f});
    sphereMesh.albedoTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                       "grey_porous_rock_40_56_diffuse.jpg";
    sphereMesh.normalTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                       "grey_porous_rock_40_56_normal.jpg";
    sphereMesh.heightTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                       "grey_porous_rock_40_56_height.jpg";
    sphereMesh.aoTextureFilename =
        "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/grey_porous_rock_40_56_ao.jpg";
    sphereMesh.metallicTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                         "grey_porous_rock_40_56_metallic.jpg";
    sphereMesh.roughnessTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                          "grey_porous_rock_40_56_roughness.jpg";

    m_mesh = std::make_shared<Mesh>();
    D3D11Utils::CreateVertexBuffer(device, sphereMesh.vertices, m_mesh->vertexBuffer);
    D3D11Utils::CreateIndexBuffer(device, sphereMesh.indices, m_mesh->indexBuffer);
    m_mesh->m_indexCount = UINT(sphereMesh.indices.size());
    m_mesh->m_vertexCount = UINT(sphereMesh.vertices.size());
    m_mesh->m_stride = UINT(sizeof(Vertex));

    D3D11Utils::CreateTexture(device, context, sphereMesh.albedoTextureFilename, true,
                              m_mesh->albedoTexture, m_mesh->albedoSRV);
    D3D11Utils::CreateTexture(device, context, sphereMesh.normalTextureFilename, false,
                              m_mesh->normalTexture, m_mesh->normalSRV);
    D3D11Utils::CreateTexture(device, context, sphereMesh.heightTextureFilename, false,
                              m_mesh->heightTexture, m_mesh->heightSRV);
    D3D11Utils::CreateTexture(device, context, sphereMesh.aoTextureFilename, false,
                              m_mesh->aoTexture, m_mesh->aoSRV);
    D3D11Utils::CreateMetallicRoughnessTexture(
        device, context, sphereMesh.metallicTextureFilename, sphereMesh.roughnessTextureFilename,
        m_mesh->metallicRoughnessTexture, m_mesh->metallicRoughnessSRV);

    // mainSphere와 동일 (GLTF와 노멀맵 Y축 반대)
    m_materialConstsCPU.invertNormalMapY = true;
    m_materialConstsCPU.useAlbedoMap = true;
    m_materialConstsCPU.useNormalMap = true;
    m_materialConstsCPU.useAOMap = true;
    m_materialConstsCPU.useMetallicMap = true;
    m_materialConstsCPU.useRoughnessMap = true;
    // heightScale 기본값 0.0이라 실제 변위는 없음(mainSphere와 동일)
    m_meshConstsCPU.useHeightMap = true;

    D3D11Utils::CreateConstBuffer(device, m_meshConstsCPU, m_meshConstsGPU);
    D3D11Utils::CreateConstBuffer(device, m_materialConstsCPU, m_materialConstsGPU);

    vector<SphereInstanceData> initialInstances(maxInstanceCount);
    D3D11Utils::CreateDynamicVertexBuffer(device, initialInstances, m_instanceBuffer);
}

void InstancedSpheres::UpdateInstances(ComPtr<ID3D11DeviceContext> &context,
                                       const PhysicsBody bodies[], int count) {
    m_instanceCount = (std::min)(count, m_maxInstanceCount);

    vector<SphereInstanceData> instances(m_instanceCount);
    for (int i = 0; i < m_instanceCount; i++) {
        // world = Scale(radius) * Rotation * Translation
        // transpose 하지 않는다: BasicInstancedVS.hlsl에서 float4x4(row0..row3)로 그대로 재구성
        instances[i].world = Matrix::CreateScale(bodies[i].radius) *
                             Matrix::CreateFromQuaternion(bodies[i].orientation) *
                             Matrix::CreateTranslation(bodies[i].position);
    }

    D3D11_MAPPED_SUBRESOURCE ms;
    context->Map(m_instanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, instances.data(), sizeof(SphereInstanceData) * instances.size());
    context->Unmap(m_instanceBuffer.Get(), 0);
}

void InstancedSpheres::Render(ComPtr<ID3D11DeviceContext> &context) {
    if (m_instanceCount <= 0) {
        return;
    }

    context->VSSetConstantBuffers(0, 1, m_meshConstsGPU.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, m_materialConstsGPU.GetAddressOf());

    context->VSSetShaderResources(0, 1, m_mesh->heightSRV.GetAddressOf());

    vector<ID3D11ShaderResourceView *> resViews = {
        m_mesh->albedoSRV.Get(), m_mesh->normalSRV.Get(), m_mesh->aoSRV.Get(),
        m_mesh->metallicRoughnessSRV.Get(), m_mesh->emissiveSRV.Get()};
    context->PSSetShaderResources(0, UINT(resViews.size()), resViews.data());

    ID3D11Buffer *vertexBuffers[2] = {m_mesh->vertexBuffer.Get(), m_instanceBuffer.Get()};
    UINT strides[2] = {UINT(sizeof(Vertex)), UINT(sizeof(SphereInstanceData))};
    UINT offsets[2] = {0, 0};
    context->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);

    context->IASetIndexBuffer(m_mesh->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->DrawIndexedInstanced(m_mesh->m_indexCount, UINT(m_instanceCount), 0, 0, 0);
}

} // namespace hlab
