#include "CubeMapping.h"

namespace hlab {

void CubeMapping::Initialize(ComPtr<ID3D11Device> &device, const wchar_t *envFilename,
                             const wchar_t *specularFilename, const wchar_t *irradianceFilename,
                             const wchar_t *brdfFilename) {

    D3D11Utils::CreateDDSTexture(device, envFilename, true, m_envSRV);
    D3D11Utils::CreateDDSTexture(device, specularFilename, true, m_specularSRV);
    D3D11Utils::CreateDDSTexture(device, irradianceFilename, true, m_irradianceSRV);

    // BRDF LookUp Table은 CubeMap이 아니라 2D 텍스춰 입니다.
    D3D11Utils::CreateDDSTexture(device, brdfFilename, false, m_brdfSRV);

    m_cubeMesh = std::make_shared<Mesh>();

    D3D11Utils::CreateConstBuffer(device, m_viewProjConstData, m_viewProjConstBuffer);
    D3D11Utils::CreateConstBuffer(device, m_mirrorViewProjConstData, m_mirrorViewProjConstBuffer);
    D3D11Utils::CreateConstBuffer(device, m_pixelConstData, m_cubeMesh->pixelConstBuffer);

    MeshData cubeMeshData = GeometryGenerator::MakeBox(40.0f);
    std::reverse(cubeMeshData.indices.begin(), cubeMeshData.indices.end());

    D3D11Utils::CreateVertexBuffer(device, cubeMeshData.vertices, m_cubeMesh->vertexBuffer);
    m_cubeMesh->m_indexCount = UINT(cubeMeshData.indices.size());
    D3D11Utils::CreateIndexBuffer(device, cubeMeshData.indices, m_cubeMesh->indexBuffer);

    vector<D3D11_INPUT_ELEMENT_DESC> basicInputElements = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    D3D11Utils::CreateVertexShaderAndInputLayout(device, L"CubeMappingVS.hlsl", basicInputElements,
                                                 m_vertexShader, m_inputLayout);

    D3D11Utils::CreatePixelShader(device, L"CubeMappingPS.hlsl", m_pixelShader);

    // Texture sampler 만들기
    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    // Create the Sample State
    device->CreateSamplerState(&sampDesc, m_samplerState.GetAddressOf());
}

void CubeMapping::UpdateViewProjConstBuffer(ComPtr<ID3D11Device> &device,
                                            ComPtr<ID3D11DeviceContext> &context,
                                            const Matrix &viewRowInput, const Matrix &projRow) {

    Matrix viewRow = viewRowInput;
    viewRow.Translation(Vector3(0.0f)); // 이동 취소

    this->m_viewProjConstData.viewProj = (viewRow * projRow).Transpose();
    //this->m_mirrorViewProjConstData.viewProj = (reflRow * viewRow * projRow).Transpose();

    D3D11Utils::UpdateBuffer(device, context, m_viewProjConstData, m_viewProjConstBuffer);
    /*D3D11Utils::UpdateBuffer(device, context, m_mirrorViewProjConstData,
                             m_mirrorViewProjConstBuffer);*/
}

void CubeMapping::UpdatePixelConstBuffer(ComPtr<ID3D11Device> &device,
                                         ComPtr<ID3D11DeviceContext> &context) {
    D3D11Utils::UpdateBuffer(device, context, m_pixelConstData, m_cubeMesh->pixelConstBuffer);
}

void CubeMapping::Render(ComPtr<ID3D11DeviceContext> &context, const bool &mirror) {

    UINT stride = sizeof(Vertex);
    UINT offset = 0;

    context->IASetInputLayout(m_inputLayout.Get());
    context->IASetVertexBuffers(0, 1, m_cubeMesh->vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_cubeMesh->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(m_vertexShader.Get(), 0, 0);

    // 거울에 그릴 때는 constBuffer의 포인터만 교체
    std::vector<ID3D11Buffer *> vertexCB = {mirror ? m_mirrorViewProjConstBuffer.Get()
                                                   : m_viewProjConstBuffer.Get()};
    context->VSSetConstantBuffers(0, UINT(vertexCB.size()), vertexCB.data());

    std::vector<ID3D11ShaderResourceView *> srvs = {m_envSRV.Get(), m_specularSRV.Get(),
                                                    m_irradianceSRV.Get()};
    context->PSSetShaderResources(0, UINT(srvs.size()), srvs.data());
    context->PSSetShader(m_pixelShader.Get(), 0, 0);
    context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, m_cubeMesh->pixelConstBuffer.GetAddressOf());

    context->DrawIndexed(m_cubeMesh->m_indexCount, 0, 0);
}

} // namespace hlab