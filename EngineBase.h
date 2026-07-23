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
//#include "D3D11Utils.h"
#include "PostProcess.h"
#include "BasicConstantData.h"
namespace hlab {

using Microsoft::WRL::ComPtr;
using std::vector;
using std::wstring;

// 모든 예제들이 공통적으로 사용할 기능들을 가지고 있는
// 부모 클래스
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

    // Convenience overrides for handling mouse input.
    virtual void OnMouseDown(WPARAM btnState, int x, int y){};
    virtual void OnMouseUp(WPARAM btnState, int x, int y){};
    /*virtual void OnMouseMove(WPARAM btnState, int x, int y);*/

    void UpdateEyeViewProjBuffers(const Vector3 &eyeWorld, const Matrix &viewRow,
                                  const Matrix &projRow, const Matrix &refl = Matrix());
  protected: // 상속 받은 클래스에서도 접근 가능
    bool InitMainWindow();
    bool InitDirect3D();
    bool InitGUI();
    void CreateBuffers();
    // void SetViewport(); 미구현
 
  public:
    // 변수 이름 붙이는 규칙은 VS DX11/12 기본 템플릿을 따릅니다.
    // 다만 변수 이름을 줄이기 위해 d3d는 생략했습니다.
    // 예: m_d3dDevice -> m_device
    int m_screenWidth; // 렌더링할 최종 화면의 해상도
    int m_screenHeight;
    HWND m_mainWindow;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGISwapChain> m_swapChain;

    // Depth buffer 관련
    ComPtr<ID3D11Texture2D> m_depthStencilBuffer;
    ComPtr<ID3D11DepthStencilView> m_depthStencilView;
    ComPtr<ID3D11DepthStencilState> m_drawDSS;       // 일반적으로 그리기
    ComPtr<ID3D11DepthStencilState> m_maskDSS;       // 스텐실버퍼에 거울 위치 표시
    ComPtr<ID3D11DepthStencilState> m_drawMaskedDSS; // 스텐실 표시된 곳(거울 안)에만 그리기
    ComPtr<ID3D11BlendState> m_mirrorBS;

     // BackBuffer
    ComPtr<ID3D11RenderTargetView> m_backBufferRTV;

    // Float MSAA Buffer (HDR 렌더링용)
    ComPtr<ID3D11Texture2D> m_floatBuffer;
    ComPtr<ID3D11ShaderResourceView> m_floatSRV;
    ComPtr<ID3D11RenderTargetView> m_floatRTV;

    // Resolved Buffer (MSAA 해제 후)
    ComPtr<ID3D11Texture2D> m_resolvedBuffer;
    ComPtr<ID3D11ShaderResourceView> m_resolvedSRV;
    ComPtr<ID3D11RenderTargetView> m_resolvedRTV;

    //와이어프레임 옵션
    bool m_isDrawAsWire = false;
    ComPtr<ID3D11RasterizerState> m_solidRS;
    ComPtr<ID3D11RasterizerState> m_solidCCWRS; // Counter-ClockWise
    ComPtr<ID3D11RasterizerState> m_wireRS;
    ComPtr<ID3D11RasterizerState> m_wireCCWRS;


    // MSAA 관련
    UINT m_numQualityLevels = 0;
    bool m_useMSAA = true;

    //후처리 필터 
    PostProcess m_postProcess;

    //카메라 클래스
    Camera m_camera;
    D3D11_VIEWPORT m_screenViewport;
    RAWINPUTDEVICE rid;
    HWND m_hwnd;
    // 현재 키보드가 눌렸는지 상태를 저장하는 배열
    bool m_keyPressed[256] = {
        false,
    };

    //마우스관련
    bool m_leftButton = false;
    bool m_rightButton = false;
    bool m_dragStartFlag = false;
    bool m_selected = false;
    float m_cursorNdcX;
    float m_cursorNdcY;

    // 마우스 좌표를 저장하는 변수 : 화면  좌표계 기준으로 저장 (picking에 사용)
    float m_virtualCursorX = 0.0f;
    float m_virtualCursorY = 0.0f;

     // 거울 구현을 더 효율적으로 하기 위해 ConstBuffer들 분리
    EyeViewProjConstData m_eyeViewProjConstData;
    EyeViewProjConstData m_mirrorEyeViewProjConstData;
    ComPtr<ID3D11Buffer> m_eyeViewProjConstBuffer;
    ComPtr<ID3D11Buffer> m_mirrorEyeViewProjConstBuffer;

    //others
    int m_guiWidth = 0;   // GUI 패널이 화면 일부를 가릴 때 뷰포트 보정용 (거울과 무관)
    bool m_useEnv = true; // 환경맵 on/off 토글
};
} // namespace hlab