
#include "BasicMeshGroup.h"
#include "GeometryGenerator.h"

namespace hlab {
void BasicMeshGroup::Initialize(ComPtr<ID3D11Device> &device, ComPtr<ID3D11DeviceContext> &context,
                                const std::string &basePath, const std::string &filename) {

    auto meshes = GeometryGenerator::ReadFromFile(basePath, filename);

    Initialize(device, context, meshes);
}

void BasicMeshGroup::Initialize(ComPtr<ID3D11Device> &device, ComPtr<ID3D11DeviceContext> &context,
                                const std::vector<MeshData> &meshes) {

    // Sampler 만들기
    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

    // Mipmap level 사이에서 Point sampling도 해보세요.
    // sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;

    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    device->CreateSamplerState(&sampDesc, m_samplerState.GetAddressOf());

    // ConstantBuffer 만들기
    m_basicVertexConstData.modelWorld = Matrix();
    m_basicVertexConstData.view = Matrix();
    m_basicVertexConstData.projection = Matrix();

    D3D11Utils::CreateConstantBuffer(device, m_basicVertexConstData, m_vertexConstantBuffer);
    D3D11Utils::CreateConstantBuffer(device, m_basicPixelConstData, m_pixelConstantBuffer);

    for (const auto &meshData : meshes) {
        auto newMesh = std::make_shared<Mesh>();
        D3D11Utils::CreateVertexBuffer(device, meshData.vertices, newMesh->vertexBuffer);
        newMesh->m_indexCount = UINT(meshData.indices.size());
        newMesh->m_vertexCount = UINT(meshData.vertices.size());

        D3D11Utils::CreateIndexBuffer(device, meshData.indices, newMesh->indexBuffer);

        if (!meshData.albedoTextureFilename.empty()) {

            std::cout << meshData.albedoTextureFilename << std::endl;
            D3D11Utils::CreateTexture(device, context, meshData.albedoTextureFilename,
                                      newMesh->albedoTexture, newMesh->albedoSRV);
        }

        if (!meshData.normalTextureFilename.empty()) {
            std::cout << meshData.normalTextureFilename << std::endl;
            D3D11Utils::CreateTexture(device, context, meshData.normalTextureFilename,
                                      newMesh->normalTexture, newMesh->normalSRV);
        }

        if (!meshData.heightTextureFilename.empty()) {
            std::cout << meshData.heightTextureFilename << std::endl;
            D3D11Utils::CreateTexture(device, context, meshData.heightTextureFilename,
                                      newMesh->heightTexture, newMesh->heightSRV);
        }

        if (!meshData.aoTextureFilename.empty()) {
            std::cout << meshData.aoTextureFilename << std::endl;
            D3D11Utils::CreateTexture(device, context, meshData.aoTextureFilename,
                                      newMesh->aoTexture, newMesh->aoSRV);
        }
        if (!meshData.metallicTextureFilename.empty()) {
            D3D11Utils::CreateTexture(device, context, meshData.metallicTextureFilename,
                                      newMesh->metallicTexture, newMesh->metallicSRV);
        }

        if (!meshData.roughnessTextureFilename.empty()) {
            D3D11Utils::CreateTexture(device, context, meshData.roughnessTextureFilename,
                                      newMesh->roughnessTexture, newMesh->roughnessSRV);
        }
        newMesh->vertexConstantBuffer = m_vertexConstantBuffer;
        newMesh->pixelConstantBuffer = m_pixelConstantBuffer;

        this->m_meshes.push_back(newMesh);
    }

    vector<D3D11_INPUT_ELEMENT_DESC> basicInputElements = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    D3D11Utils::CreateVertexShaderAndInputLayout(device, L"BasicVS.hlsl", basicInputElements,
                                                 m_basicVertexShader, m_basicInputLayout);

    D3D11Utils::CreatePixelShader(device, L"BasicPS.hlsl", m_basicPixelShader);

    // Geometry shader 초기화하기
    D3D11Utils::CreateGeometryShader(device, L"NormalGS.hlsl", m_normalGeometryShader);

    D3D11Utils::CreateVertexShaderAndInputLayout(device, L"NormalVS.hlsl", basicInputElements,
                                                 m_normalVertexShader, m_basicInputLayout);
    D3D11Utils::CreatePixelShader(device, L"NormalPS.hlsl", m_normalPixelShader);

    D3D11Utils::CreateConstantBuffer(device, m_normalVertexConstantData,
                                     m_normalVertexConstantBuffer);
}

void BasicMeshGroup::UpdateConstantBuffers(ComPtr<ID3D11Device> &device,
                                           ComPtr<ID3D11DeviceContext> &context) {

    D3D11Utils::UpdateBuffer(device, context, m_basicVertexConstData, m_vertexConstantBuffer);

    D3D11Utils::UpdateBuffer(device, context, m_basicPixelConstData, m_pixelConstantBuffer);

    // 노멀 벡터 그리기
    if (m_drawNormals && m_drawNormalsDirtyFlag) {
        D3D11Utils::UpdateBuffer(device, context, m_normalVertexConstantData,
                                 m_normalVertexConstantBuffer);
        m_drawNormalsDirtyFlag = false;
    }
}

void BasicMeshGroup::Render(ComPtr<ID3D11DeviceContext> &context) {
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    for (const auto &mesh : m_meshes) {
        context->VSSetShader(m_basicVertexShader.Get(), 0, 0);

        // VertexShader에서도 Texture 사용
        context->VSSetShaderResources(0, 1, mesh->heightSRV.GetAddressOf());
        context->VSSetSamplers(0, 1, m_samplerState.GetAddressOf());
        context->VSSetConstantBuffers(0, 1, mesh->vertexConstantBuffer.GetAddressOf());

        context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
        context->PSSetShader(m_basicPixelShader.Get(), 0, 0);
        /*
        TextureCube specularIBLTex : register(t0); //specular에 사용
        TextureCube irradianceIBLTex : register(t1); //diffuse에 사용
        Texture2D brdfTex : register(t2); //
        Texture2D albedoTex : register(t3);
        Texture2D normalTex : register(t4);
        Texture2D aoTex : register(t5);
        Texture2D metallicTex : register(t6);
        Texture2D roughnessTex : register(t7);
        */
        // 물체 렌더링할 때 여러가지 텍스춰 사용
        vector<ID3D11ShaderResourceView *> resViews = {
            m_specularSRV.Get(),     m_irradianceSRV.Get(),   m_brdfSRV.Get(),
            mesh->albedoSRV.Get(),   mesh->normalSRV.Get(),   mesh->aoSRV.Get(),
            mesh->metallicSRV.Get(), mesh->roughnessSRV.Get()};
        /*vector<ID3D11ShaderResourceView *> resViews = {
            nullptr, nullptr, mesh->albedoSRV.Get(),
            mesh->normalSRV.Get(), mesh->aoSRV.Get()};*/
        context->PSSetShaderResources(0, UINT(resViews.size()), resViews.data());

        context->PSSetConstantBuffers(0, 1, mesh->pixelConstantBuffer.GetAddressOf());

        context->IASetInputLayout(m_basicInputLayout.Get());
        context->IASetVertexBuffers(0, 1, mesh->vertexBuffer.GetAddressOf(), &stride, &offset);
        context->IASetIndexBuffer(mesh->indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->DrawIndexed(mesh->m_indexCount, 0, 0);

        if (m_drawNormals) {
            // 같은 VertexBuffer 사용
            context->VSSetShader(m_normalVertexShader.Get(), 0, 0);
            ID3D11Buffer *pptr[2] = {m_vertexConstantBuffer.Get(),
                                     m_normalVertexConstantBuffer.Get()};
            context->GSSetConstantBuffers(0, 2, pptr);
            context->GSSetShader(m_normalGeometryShader.Get(), 0, 0);
            context->PSSetShader(m_normalPixelShader.Get(), 0, 0);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
            context->Draw(mesh->m_vertexCount, 0);
            context->GSSetShader(nullptr, 0, 0);
        }
    }
}

void BasicMeshGroup::UpdateModelWorld(const Matrix &modelWorldRow) {
    this->m_modelWorldRow = modelWorldRow;
    this->m_invTransposeRow = modelWorldRow;
    m_invTransposeRow.Translation(Vector3(0.0f));
    m_invTransposeRow = m_invTransposeRow.Invert().Transpose();

    m_basicVertexConstData.modelWorld = modelWorldRow.Transpose();
    m_basicVertexConstData.invTranspose = m_invTransposeRow.Transpose();
}
} // namespace hlab