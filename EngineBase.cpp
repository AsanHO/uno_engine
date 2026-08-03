#include "EngineBase.h"

#include <algorithm>
#include <directxtk/SimpleMath.h>

#include "D3D11Utils.h"
#include "GraphicsCommon.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

namespace hlab {

using namespace std;

EngineBase *g_appBase = nullptr;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    return g_appBase->MsgProc(hWnd, msg, wParam, lParam);
}

EngineBase::EngineBase()
    : m_screenWidth(1280), m_screenHeight(720), m_mainWindow(0),
      m_screenViewport(D3D11_VIEWPORT()) {
    g_appBase = this;
}

EngineBase::~EngineBase() {
    g_appBase = nullptr;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroyWindow(m_mainWindow);
}

float EngineBase::GetAspectRatio() const { return float(m_screenWidth) / m_screenHeight; }

int EngineBase::Run() {
    MSG msg = {0};
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            ImGui::Begin("Scene Control");
            ImGui::Text("Average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate);
            UpdateGUI();
            ImGui::End();
            ImGui::Render();

            Update(ImGui::GetIO().DeltaTime);
            Render();

            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            m_swapChain->Present(1, 0);
        }
    }
    return 0;
}

bool EngineBase::Initialize() {
    if (!InitMainWindow())
        return false;
    if (!InitDirect3D())
        return false;
    if (!InitGUI())
        return false;
    return true;
}

// ---- 신규: GlobalConstants 갱신 (기존 UpdateEyeViewProjBuffers 대체) ----
void EngineBase::UpdateGlobalConstants(const Vector3 &eyeWorld, const Matrix &viewRow,
                                       const Matrix &projRow, const Matrix &refl) {
    m_globalConstsCPU.eyeWorld = eyeWorld;
    m_globalConstsCPU.view = viewRow.Transpose();
    m_globalConstsCPU.proj = projRow.Transpose();
    //신규->
    m_globalConstsCPU.invProj = projRow.Invert().Transpose();
    //<-
    m_globalConstsCPU.viewProj = (viewRow * projRow).Transpose();

        // 신규->
    m_globalConstsCPU.invViewProj = m_globalConstsCPU.viewProj.Invert();
    //<-
    // 반사 버전은 CPU 구조체를 통째로 복사한 뒤 view/viewProj만 교체
    m_reflectGlobalConstsCPU = m_globalConstsCPU;
    // 신규->
    memcpy(&m_reflectGlobalConstsCPU, &m_globalConstsCPU, sizeof(m_globalConstsCPU));
    //<-
    m_reflectGlobalConstsCPU.view = (refl * viewRow).Transpose();
    m_reflectGlobalConstsCPU.viewProj = (refl * viewRow * projRow).Transpose();
    // 신규->
    m_reflectGlobalConstsCPU.invViewProj = m_reflectGlobalConstsCPU.viewProj.Invert();
    //<-
    D3D11Utils::UpdateBuffer(m_device, m_context, m_globalConstsCPU, m_globalConstsGPU);
    D3D11Utils::UpdateBuffer(m_device, m_context, m_reflectGlobalConstsCPU,
                             m_reflectGlobalConstsGPU);
}

// ---- 신규: GlobalConstants를 b1에 바인딩 ----
void EngineBase::SetGlobalConsts(ComPtr<ID3D11Buffer> &globalConstsGPU) {
    // Common.hlsli와 일관성 유지: register(b1)
    m_context->VSSetConstantBuffers(1, 1, globalConstsGPU.GetAddressOf());
    m_context->PSSetConstantBuffers(1, 1, globalConstsGPU.GetAddressOf());
    m_context->GSSetConstantBuffers(1, 1, globalConstsGPU.GetAddressOf());
}

void EngineBase::CreateDepthBuffers() {

    // DepthStencilView 만들기
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = m_screenWidth;
    desc.Height = m_screenHeight;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    if (m_useMSAA && m_numQualityLevels > 0) {
        desc.SampleDesc.Count = 4;
        desc.SampleDesc.Quality = m_numQualityLevels - 1;
    } else {
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
    }
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    ComPtr<ID3D11Texture2D> depthStencilBuffer;
    ThrowIfFailed(m_device->CreateTexture2D(&desc, 0, depthStencilBuffer.GetAddressOf()));
    ThrowIfFailed(m_device->CreateDepthStencilView(depthStencilBuffer.Get(), NULL,
                                                   m_depthStencilView.GetAddressOf()));

    // Depth 전용
    desc.Format = DXGI_FORMAT_R32_TYPELESS;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    ThrowIfFailed(m_device->CreateTexture2D(&desc, NULL, m_depthOnlyBuffer.GetAddressOf()));

    // 그림자 Buffers (Depth 전용)
    desc.Width = m_shadowWidth;
    desc.Height = m_shadowHeight;
    for (int i = 0; i < MAX_LIGHTS; i++) {
        ThrowIfFailed(m_device->CreateTexture2D(&desc, NULL, m_shadowBuffers[i].GetAddressOf()));
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    ZeroMemory(&dsvDesc, sizeof(dsvDesc));
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    ThrowIfFailed(m_device->CreateDepthStencilView(m_depthOnlyBuffer.Get(), &dsvDesc,
                                                   m_depthOnlyDSV.GetAddressOf()));

    // 그림자 DSVs
    for (int i = 0; i < MAX_LIGHTS; i++) {
        ThrowIfFailed(m_device->CreateDepthStencilView(m_shadowBuffers[i].Get(), &dsvDesc,
                                                       m_shadowDSVs[i].GetAddressOf()));
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    ThrowIfFailed(m_device->CreateShaderResourceView(m_depthOnlyBuffer.Get(), &srvDesc,
                                                     m_depthOnlySRV.GetAddressOf()));

    // 그림자 SRVs
    for (int i = 0; i < MAX_LIGHTS; i++) {
        ThrowIfFailed(m_device->CreateShaderResourceView(m_shadowBuffers[i].Get(), &srvDesc,
                                                         m_shadowSRVs[i].GetAddressOf()));
    }
}
// ---- 신규: PSO 한 번에 적용 ----
void EngineBase::SetPipelineState(const GraphicsPSO &pso) {
    m_context->VSSetShader(pso.m_vertexShader.Get(), 0, 0);
    m_context->PSSetShader(pso.m_pixelShader.Get(), 0, 0);
    m_context->HSSetShader(pso.m_hullShader.Get(), 0, 0);
    m_context->DSSetShader(pso.m_domainShader.Get(), 0, 0);
    m_context->GSSetShader(pso.m_geometryShader.Get(), 0, 0);
    m_context->IASetInputLayout(pso.m_inputLayout.Get());
    m_context->RSSetState(pso.m_rasterizerState.Get());
    m_context->OMSetBlendState(pso.m_blendState.Get(), pso.m_blendFactor, 0xffffffff);
    m_context->OMSetDepthStencilState(pso.m_depthStencilState.Get(), pso.m_stencilRef);
    m_context->IASetPrimitiveTopology(pso.m_primitiveTopology);
}

// ---- 신규: 공용 IBL 텍스처 로딩 ----
void EngineBase::InitCubemaps(wstring basePath, wstring envFilename, wstring specularFilename,
                              wstring irradianceFilename, wstring brdfFilename) {
    D3D11Utils::CreateDDSTexture(m_device, (basePath + envFilename).c_str(), true, m_envSRV);
    D3D11Utils::CreateDDSTexture(m_device, (basePath + specularFilename).c_str(), true,
                                 m_specularSRV);
    D3D11Utils::CreateDDSTexture(m_device, (basePath + irradianceFilename).c_str(), true,
                                 m_irradianceSRV);
    D3D11Utils::CreateDDSTexture(m_device, (basePath + brdfFilename).c_str(), false, m_brdfSRV);
}

LRESULT EngineBase::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        break;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_INPUT: {
        UINT dataSize = 0;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dataSize, sizeof(RAWINPUTHEADER));
        std::vector<BYTE> buffer(dataSize);
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer.data(), &dataSize,
                            sizeof(RAWINPUTHEADER)) != dataSize) {
            break;
        }
        RAWINPUT *raw = reinterpret_cast<RAWINPUT *>(buffer.data());
        if (raw->header.dwType == RIM_TYPEMOUSE) {
            LONG dx = raw->data.mouse.lLastX;
            LONG dy = raw->data.mouse.lLastY;
            if (m_leftButton || m_rightButton) {
                const float sensitivity = 0.7f;
                m_virtualCursorX += float(dx) * sensitivity;
                m_virtualCursorY += float(dy) * sensitivity;
                m_virtualCursorX = std::clamp(m_virtualCursorX, 0.0f, float(m_screenWidth));
                m_virtualCursorY = std::clamp(m_virtualCursorY, 0.0f, float(m_screenHeight));
                m_cursorNdcX = m_virtualCursorX * 2.0f / m_screenWidth - 1.0f;
                m_cursorNdcY = -m_virtualCursorY * 2.0f / m_screenHeight + 1.0f;
            } else {
                m_camera.UpdateMouse(static_cast<float>(dx), static_cast<float>(dy));
            }
        }
        break;
    }
    case WM_LBUTTONDOWN:
        if (!m_leftButton) {
            m_dragStartFlag = true;
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            m_virtualCursorX = float(pt.x);
            m_virtualCursorY = float(pt.y);
            m_cursorNdcX = m_virtualCursorX * 2.0f / m_screenWidth - 1.0f;
            m_cursorNdcY = -m_virtualCursorY * 2.0f / m_screenHeight + 1.0f;
            m_cursorNdcX = std::clamp(m_cursorNdcX, -1.0f, 1.0f);
            m_cursorNdcY = std::clamp(m_cursorNdcY, -1.0f, 1.0f);
        }
        m_leftButton = true;
        break;
    case WM_LBUTTONUP:
        m_leftButton = false;
        break;
    case WM_RBUTTONDOWN:
        if (!m_rightButton) {
            m_dragStartFlag = true;
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hwnd, &pt);
            m_virtualCursorX = float(pt.x);
            m_virtualCursorY = float(pt.y);
            m_cursorNdcX = m_virtualCursorX * 2.0f / m_screenWidth - 1.0f;
            m_cursorNdcY = -m_virtualCursorY * 2.0f / m_screenHeight + 1.0f;
            m_cursorNdcX = std::clamp(m_cursorNdcX, -1.0f, 1.0f);
            m_cursorNdcY = std::clamp(m_cursorNdcY, -1.0f, 1.0f);
        }
        m_rightButton = true;
        break;
    case WM_RBUTTONUP:
        m_rightButton = false;
        break;
    case WM_KEYDOWN:
        m_keyPressed[wParam] = true;
        if (wParam == 27) {
            DestroyWindow(hwnd);
        }
        break;
    case WM_KEYUP:
        if (wParam == 'F') {
            m_camera.m_isUseFirstPersonView = !m_camera.m_isUseFirstPersonView;
        }
        m_keyPressed[wParam] = false;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }

    return ::DefWindowProc(hwnd, msg, wParam, lParam);
}

bool EngineBase::InitMainWindow() {
    WNDCLASSEX wc = {sizeof(WNDCLASSEX),    CS_CLASSDC, WndProc, 0L,   0L,
                     GetModuleHandle(NULL), NULL,       NULL,    NULL, NULL,
                     L"HongLabGraphics",    NULL};

    if (!RegisterClassEx(&wc)) {
        cout << "RegisterClassEx() failed." << endl;
        return false;
    }

    RECT wr = {0, 0, m_screenWidth, m_screenHeight};
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, false);
    m_mainWindow =
        CreateWindow(wc.lpszClassName, L"HongLabGraphics Example", WS_OVERLAPPEDWINDOW, 100, 100,
                     wr.right - wr.left, wr.bottom - wr.top, NULL, NULL, wc.hInstance, NULL);

    if (!m_mainWindow) {
        cout << "CreateWindow() failed." << endl;
        return false;
    }

    ShowWindow(m_mainWindow, SW_SHOWDEFAULT);
    UpdateWindow(m_mainWindow);

    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.dwFlags = 0;
    rid.hwndTarget = m_hwnd;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));

    return true;
}

bool EngineBase::InitDirect3D() {
    const D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;

    UINT createDeviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    const D3D_FEATURE_LEVEL featureLevels[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_9_3};
    D3D_FEATURE_LEVEL featureLevel;

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferDesc.Width = m_screenWidth;
    sd.BufferDesc.Height = m_screenHeight;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferCount = 2;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_mainWindow;
    sd.Windowed = TRUE;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;

    ThrowIfFailed(D3D11CreateDeviceAndSwapChain(0, driverType, 0, createDeviceFlags, featureLevels,
                                                1, D3D11_SDK_VERSION, &sd,
                                                m_swapChain.GetAddressOf(), m_device.GetAddressOf(),
                                                &featureLevel, m_context.GetAddressOf()));

    if (featureLevel != D3D_FEATURE_LEVEL_11_0) {
        cout << "D3D Feature Level 11 unsupported." << endl;
        return false;
    }

    // ---- 핵심 변화: 기존 RS/DSS/BS 개별 생성 코드 전부 삭제, 이 한 줄로 대체 ----
    Graphics::InitCommonStates(m_device);

       CreateBuffers();
    ZeroMemory(&m_screenViewport, sizeof(D3D11_VIEWPORT));
    m_screenViewport.TopLeftX = 0;
    m_screenViewport.TopLeftY = 0;
    m_screenViewport.Width = float(m_screenWidth);
    m_screenViewport.Height = float(m_screenHeight);
    m_screenViewport.MinDepth = 0.0f;
    m_screenViewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &m_screenViewport);

    // ---- 신규: GlobalConstants 버퍼 생성 ----
    D3D11Utils::CreateConstBuffer(m_device, m_globalConstsCPU, m_globalConstsGPU);
    D3D11Utils::CreateConstBuffer(m_device, m_reflectGlobalConstsCPU, m_reflectGlobalConstsGPU);


     // 그림자맵 렌더링할 때 사용할 GlobalConsts들 별도 생성
    for (int i = 0; i < MAX_LIGHTS; i++) {
        D3D11Utils::CreateConstBuffer(m_device, m_shadowGlobalConstsCPU[i],
                                      m_shadowGlobalConstsGPU[i]);
    }

    //// 후처리 효과용 ConstBuffer
    //D3D11Utils::CreateConstBuffer(m_device, m_postEffectsConstsCPU, m_postEffectsConstsGPU);

    return true;
}

bool EngineBase::InitGUI() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.DisplaySize = ImVec2(float(m_screenWidth), float(m_screenHeight));
    ImGui::StyleColorsLight();

    if (!ImGui_ImplDX11_Init(m_device.Get(), m_context.Get())) {
        return false;
    }
    if (!ImGui_ImplWin32_Init(m_mainWindow)) {
        return false;
    }
    return true;
}
void EngineBase::SetMainViewport() {

    // Set the viewport
    ZeroMemory(&m_screenViewport, sizeof(D3D11_VIEWPORT));
    m_screenViewport.TopLeftX = 0;
    m_screenViewport.TopLeftY = 0;
    m_screenViewport.Width = float(m_screenWidth);
    m_screenViewport.Height = float(m_screenHeight);
    m_screenViewport.MinDepth = 0.0f;
    m_screenViewport.MaxDepth = 1.0f;

    m_context->RSSetViewports(1, &m_screenViewport);
}

void EngineBase::SetShadowViewport() {

    // Set the viewport
    D3D11_VIEWPORT shadowViewport;
    ZeroMemory(&shadowViewport, sizeof(D3D11_VIEWPORT));
    shadowViewport.TopLeftX = 0;
    shadowViewport.TopLeftY = 0;
    shadowViewport.Width = float(m_shadowWidth);
    shadowViewport.Height = float(m_shadowHeight);
    shadowViewport.MinDepth = 0.0f;
    shadowViewport.MaxDepth = 1.0f;

    m_context->RSSetViewports(1, &shadowViewport);
}
void EngineBase::CreateBuffers() {
    ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())));
    ThrowIfFailed(
        m_device->CreateRenderTargetView(backBuffer.Get(), NULL, m_backBufferRTV.GetAddressOf()));

    ThrowIfFailed(m_device->CheckMultisampleQualityLevels(DXGI_FORMAT_R16G16B16A16_FLOAT, 4,
                                                          &m_numQualityLevels));

    D3D11_TEXTURE2D_DESC desc;
    backBuffer->GetDesc(&desc);
    desc.MipLevels = desc.ArraySize = 1;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.MiscFlags = 0;
    desc.CPUAccessFlags = 0;
    if (m_useMSAA && m_numQualityLevels) {
        desc.SampleDesc.Count = 4;
        desc.SampleDesc.Quality = m_numQualityLevels - 1;
    } else {
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
    }

    ThrowIfFailed(m_device->CreateTexture2D(&desc, NULL, m_floatBuffer.GetAddressOf()));
    ThrowIfFailed(
        m_device->CreateShaderResourceView(m_floatBuffer.Get(), NULL, m_floatSRV.GetAddressOf()));
    ThrowIfFailed(
        m_device->CreateRenderTargetView(m_floatBuffer.Get(), NULL, m_floatRTV.GetAddressOf()));

    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    ThrowIfFailed(m_device->CreateTexture2D(&desc, NULL, m_resolvedBuffer.GetAddressOf()));
    ThrowIfFailed(m_device->CreateShaderResourceView(m_resolvedBuffer.Get(), NULL,
                                                     m_resolvedSRV.GetAddressOf()));
    ThrowIfFailed(m_device->CreateRenderTargetView(m_resolvedBuffer.Get(), NULL,
                                                   m_resolvedRTV.GetAddressOf()));

     CreateDepthBuffers();

    m_postProcess.Initialize(m_device, m_context, {m_resolvedSRV}, {m_backBufferRTV}, m_screenWidth,
                             m_screenHeight, 4);
}

} // namespace hlab