#pragma once

#include <string>
#include <wrl/client.h>

#include "D3D11Utils.h"
#include "GeometryGenerator.h"
#include "Material.h"
#include "Vertex.h"

namespace hlab {

using Microsoft::WRL::ComPtr;

class CubeMapping {
  public:
    void Initialize(ComPtr<ID3D11Device> &device, const wchar_t *originalFilename,
                    const wchar_t *diffuseFilename, const wchar_t *specularFilename,
                    const wchar_t *brdfFilename);

    void UpdateViewProjConstBuffer(ComPtr<ID3D11Device> &device,
                                   ComPtr<ID3D11DeviceContext> &context, const Matrix &viewRow,
                                   const Matrix &projRow);

    void UpdatePixelConstBuffer(ComPtr<ID3D11Device> &device, ComPtr<ID3D11DeviceContext> &context);

    void Render(ComPtr<ID3D11DeviceContext> &context, const bool &mirror);

  public:
    struct ViewProjConstData {
        Matrix viewProj; // 미리 곱해서 사용
    };

    struct PixelConstData {
        int textureToDraw = 0; // 0: Env, 1: Specular, 2: Irradiance
        float mipLevel = 0.0f;
        float dummy1;
        float dummy2;
    };

  public:
    ComPtr<ID3D11ShaderResourceView> m_envSRV;
    ComPtr<ID3D11ShaderResourceView> m_specularSRV;   // Radiance
    ComPtr<ID3D11ShaderResourceView> m_irradianceSRV; // Diffuse
    ComPtr<ID3D11ShaderResourceView> m_brdfSRV;       // BRDF LookUpTable

    CubeMapping::ViewProjConstData m_viewProjConstData;
    CubeMapping::ViewProjConstData m_mirrorViewProjConstData;
    ComPtr<ID3D11Buffer> m_viewProjConstBuffer;
    ComPtr<ID3D11Buffer> m_mirrorViewProjConstBuffer;

    CubeMapping::PixelConstData m_pixelConstData;

    // IBL을 위해 다른 물체들 그릴때 사용
    ComPtr<ID3D11ShaderResourceView> m_diffuseResView;
    ComPtr<ID3D11ShaderResourceView> m_specularResView;

  private:
    std::shared_ptr<Mesh> m_cubeMesh;

    ComPtr<ID3D11SamplerState> m_samplerState;

    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
};
} // namespace hlab