#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <iostream>
#include <vector>
#include <windows.h>
#include <wrl.h> // ComPtr

#include "Camera.h"
#include "ConstantBuffers.h" // BasicConstantData.h 대체
#include "GraphicsCommon.h"
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

  protected:
    bool InitMainWindow();
    bool InitDirect3D();
    bool InitGUI();
    void CreateBuffers();

  public:
    int m_screenWidth;
    int m_screenHeight;
    HWND m_mainWindow;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;

    // Depth buffer (DSS 자체는 Graphics::로 이동, View만 여기 유지)
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;

    ComPtr<ID3D11RenderTargetView> m_backBufferRTV;

    ComPtr<ID3D11Texture2D> m_floatBuffer;
    ComPtr<ID3D11ShaderResourceView> m_floatSRV;
    ComPtr<ID3D11RenderTargetView> m_floatRTV;

    ComPtr<ID3D11Texture2D> m_resolvedBuffer;
    ComPtr<ID3D11ShaderResourceView> m_resolvedSRV;
    ComPtr<ID3D11RenderTargetView> m_resolvedRTV;

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
    ComPtr<ID3D11Buffer> m_globalConstsGPU;
    ComPtr<ID3D11Buffer> m_reflectGlobalConstsGPU;

    // ---- 신규: 공용 IBL 텍스처 (Common.hlsli의 t10~13에 바인딩됨) ----
    ComPtr<ID3D11ShaderResourceView> m_envSRV;
    ComPtr<ID3D11ShaderResourceView> m_specularSRV;
    ComPtr<ID3D11ShaderResourceView> m_irradianceSRV;
    ComPtr<ID3D11ShaderResourceView> m_brdfSRV;

    int m_guiWidth = 0;
    bool m_useEnv = true;
};
} // namespace hlab