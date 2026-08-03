#pragma once

#include <directxtk/SimpleMath.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <iostream>
#include <vector>

#include "Camera.h"
#include "ConstantBuffers.h"
#include "D3D11Utils.h"
#include "GraphicsPSO.h"
#include "PostProcess.h"

namespace hlab {

using Microsoft::WRL::ComPtr;
using std::vector;
using std::wstring;

class EngineBase {
  public:
    EngineBase();
    virtual ~EngineBase();

    float GetAspectRatio() const;
    int Run();

    virtual bool Initialize();
    virtual void UpdateGUI() = 0;
    virtual void Update(float dt) = 0;
    virtual void Render() = 0;

    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    virtual void OnMouseDown(WPARAM btnState, int x, int y) {};
    virtual void OnMouseUp(WPARAM btnState, int x, int y) {};

    // ---- 신규: EyeViewProj 관련 함수 대체 ----
    void UpdateGlobalConstants(const Vector3 &eyeWorld, const Matrix &viewRow,
                               const Matrix &projRow, const Matrix &refl = Matrix());
    void SetGlobalConsts(ComPtr<ID3D11Buffer> &globalConstsGPU);
    void SetPipelineState(const GraphicsPSO &pso);

    // ---- 신규: 공용 IBL 텍스처 로딩 ----
    void InitCubemaps(wstring basePath, wstring envFilename, wstring specularFilename,
                      wstring irradianceFilename, wstring brdfFilename);
    void CreateDepthBuffers();
  protected:
    bool InitMainWindow();
    bool InitDirect3D();
    bool InitGUI();
    void CreateBuffers();
    void SetMainViewport();
    void SetShadowViewport();

  public:
    int m_screenWidth;
    int m_screenHeight;
    HWND m_mainWindow;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;

    ComPtr<ID3D11RenderTargetView> m_backBufferRTV;

    ComPtr<ID3D11Texture2D> m_floatBuffer;
    ComPtr<ID3D11ShaderResourceView> m_floatSRV;
    ComPtr<ID3D11RenderTargetView> m_floatRTV;

    ComPtr<ID3D11Texture2D> m_resolvedBuffer;
    ComPtr<ID3D11ShaderResourceView> m_resolvedSRV;
    ComPtr<ID3D11RenderTargetView> m_resolvedRTV;
    // Depth buffer 관련
    ComPtr<ID3D11Texture2D> m_depthOnlyBuffer; // No MSAA
    ComPtr<ID3D11DepthStencilView> m_depthOnlyDSV;
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;
    ComPtr<ID3D11ShaderResourceView> m_depthOnlySRV;

    // Shadow maps
    int m_shadowWidth = 1280;
    int m_shadowHeight = 1280;
    ComPtr<ID3D11Texture2D> m_shadowBuffers[MAX_LIGHTS]; // No MSAA
    ComPtr<ID3D11DepthStencilView> m_shadowDSVs[MAX_LIGHTS];
    ComPtr<ID3D11ShaderResourceView> m_shadowSRVs[MAX_LIGHTS];

    bool m_isDrawAsWire = false;

    UINT m_numQualityLevels = 0;
    bool m_useMSAA = true;

    PostProcess m_postProcess;
    Camera m_camera;
    D3D11_VIEWPORT m_screenViewport;
    RAWINPUTDEVICE rid;
    HWND m_hwnd;

    bool m_keyPressed[256] = {false};

    bool m_leftButton = false;
    bool m_rightButton = false;
    bool m_dragStartFlag = false;
    bool m_selected = false;
    float m_cursorNdcX = 0.0f;
    float m_cursorNdcY = 0.0f;
    float m_virtualCursorX = 0.0f;
    float m_virtualCursorY = 0.0f;

    // ---- 신규: GlobalConstants (기존 EyeViewProjConstData 대체) ----
    GlobalConstants m_globalConstsCPU;
    GlobalConstants m_reflectGlobalConstsCPU;
    GlobalConstants m_shadowGlobalConstsCPU[MAX_LIGHTS];
    ComPtr<ID3D11Buffer> m_globalConstsGPU;
    ComPtr<ID3D11Buffer> m_reflectGlobalConstsGPU;
    ComPtr<ID3D11Buffer> m_shadowGlobalConstsGPU[MAX_LIGHTS];

    // ---- 신규: 공용 IBL 텍스처 (Common.hlsli의 t10~13에 바인딩됨) ----
    ComPtr<ID3D11ShaderResourceView> m_envSRV;
    ComPtr<ID3D11ShaderResourceView> m_specularSRV;
    ComPtr<ID3D11ShaderResourceView> m_irradianceSRV;
    ComPtr<ID3D11ShaderResourceView> m_brdfSRV;

    int m_guiWidth = 0;
    bool m_useEnv = true;

    bool m_lightRotate = false;
};
} // namespace hlab