// 참고: 헤더 include 순서
// https://google.github.io/styleguide/cppguide.html#Names_and_Order_of_Includes

#include "EngineBase.h"
#include <algorithm>
#include <dxgi.h>    // DXGIFactory
#include <dxgi1_4.h> // DXGIFactory4

// imgui_impl_win32.cpp에 정의된 메시지 처리 함수에 대한 전방 선언
// VCPKG를 통해 IMGUI를 사용할 경우 빨간줄로 경고가 뜰 수 있음
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

namespace hlab {

using namespace std;

// RegisterClassEx()에서 멤버 함수를 직접 등록할 수가 없기 때문에
// 클래스의 멤버 함수에서 간접적으로 메시지를 처리할 수 있도록 도와줍니다.
EngineBase *g_appBase = nullptr;

// RegisterClassEx()에서 실제로 등록될 콜백 함수
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

    // g_appBase를 이용해서 간접적으로 멤버 함수 호출
    return g_appBase->MsgProc(hWnd, msg, wParam, lParam);
}

// 생성자
EngineBase::EngineBase()
    : m_screenWidth(1280), m_screenHeight(720), m_mainWindow(0),
      m_screenViewport(D3D11_VIEWPORT()) {

    g_appBase = this;
}

EngineBase::~EngineBase() {
    g_appBase = nullptr;

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    DestroyWindow(m_mainWindow);
    // UnregisterClass(wc.lpszClassName, wc.hInstance);//생략

    // COMPtr에서 알아서 release
    // ComPtr automatically maintains a reference count for the underlying
    // interface pointer and releases the interface when the reference count
    // goes to zero.
    // https:learn.microsoft.com/en-us/cpp/cppcx/wrl/comptr-class?view=msvc-170
    // 예시: m_d3dDevice.Reset(); 생략
}

float EngineBase::GetAspectRatio() const { return float(m_screenWidth) / m_screenHeight; }

int EngineBase::Run() {

    // Main message loop
    MSG msg = {0};
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            ImGui_ImplDX11_NewFrame(); // GUI 프레임 시작
            ImGui_ImplWin32_NewFrame();

            ImGui::NewFrame(); // 어떤 것들을 렌더링 할지 기록 시작
            ImGui::Begin("Scene Control");

            // ImGui가 측정해주는 Framerate 출력
            ImGui::Text("Average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate);

            UpdateGUI(); // 추가적으로 사용할 GUI

            ImGui::End();
            ImGui::Render(); // 렌더링할 것들 기록 끝

            Update(ImGui::GetIO().DeltaTime); // 애니메이션 같은 변화

            Render(); // 우리가 구현한 렌더링

            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); // GUI 렌더링

            // Switch the back buffer and the front buffer
            // 주의: ImGui RenderDrawData() 다음에 Present() 호출
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

LRESULT EngineBase::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        // Reset and resize swapchain
        break;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    /*case WM_MOUSEMOVE:
        cout << "Mouse " << LOWORD(lParam) << " " << HIWORD(lParam) << endl;
        OnMouseMove(wParam, LOWORD(lParam), HIWORD(lParam));
        break;*/
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
            cout << dx << " " << dy << endl;
            m_camera.UpdateMouse(static_cast<float>(dx), static_cast<float>(dy));
        }

        break;
    }
    case WM_LBUTTONUP:
        cout << "WM_LBUTTONUP Left mouse button" << endl;
        break;
    case WM_RBUTTONUP:
        cout << "WM_RBUTTONUP Right mouse button" << endl;
        break;
    case WM_KEYDOWN:
        cout << "WM_KEYDOWN " << (int)wParam << endl;
        m_keyPressed[wParam] = true;
        if (wParam == 27) {
            // ESC 키가 눌렸을 때 프로그램 종료
            DestroyWindow(hwnd);
        }
        break;
    case WM_KEYUP:
        // 키보드가 눌린 상태인지 아닌지 저장
        if (wParam == 'F') { // f키 일인칭 시점
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

    WNDCLASSEX wc = {sizeof(WNDCLASSEX),
                     CS_CLASSDC,
                     WndProc,
                     0L,
                     0L,
                     GetModuleHandle(NULL),
                     NULL,
                     NULL,
                     NULL,
                     NULL,
                     L"HongLabGraphics", // lpszClassName, L-string
                     NULL};

    // The RegisterClass function has been superseded by the RegisterClassEx function.
    // https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerclassa?redirectedfrom=MSDN
    if (!RegisterClassEx(&wc)) {
        cout << "RegisterClassEx() failed." << endl;
        return false;
    }

    // 툴바까지 포함한 윈도우 전체 해상도가 아니라
    // 우리가 실제로 그리는 해상도가 width x height가 되도록
    // 윈도우를 만들 해상도를 다시 계산해서 CreateWindow()에서 사용

    // 우리가 원하는 그림이 그려질 부분의 해상도
    RECT wr = {0, 0, m_screenWidth, m_screenHeight};

    // 필요한 윈도우 크기(해상도) 계산
    // wr의 값이 바뀜
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, false);

    // 윈도우를 만들때 위에서 계산한 wr 사용
    m_mainWindow = CreateWindow(wc.lpszClassName, L"HongLabGraphics Example", WS_OVERLAPPEDWINDOW,
                                100,                // 윈도우 좌측 상단의 x 좌표
                                100,                // 윈도우 좌측 상단의 y 좌표
                                wr.right - wr.left, // 윈도우 가로 방향 해상도
                                wr.bottom - wr.top, // 윈도우 세로 방향 해상도
                                NULL, NULL, wc.hInstance, NULL);

    if (!m_mainWindow) {
        cout << "CreateWindow() failed." << endl;
        return false;
    }

    ShowWindow(m_mainWindow, SW_SHOWDEFAULT);
    UpdateWindow(m_mainWindow);
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02; // Mouse
    rid.dwFlags = 0;
    rid.hwndTarget = m_hwnd;

    RegisterRawInputDevices(&rid, 1, sizeof(rid));
    return true;
}

bool EngineBase::InitDirect3D() {

    // 이 예제는 Intel 내장 그래픽스 칩으로 실행을 확인하였습니다.
    // (LG 그램, 17Z90n, Intel Iris Plus Graphics)
    // 만약 그래픽스 카드 호환성 문제로 D3D11CreateDevice()가 실패하는 경우에는
    // D3D_DRIVER_TYPE_HARDWARE 대신 D3D_DRIVER_TYPE_WARP 사용해보세요
    // const D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_WARP; //CPU를 사용하겠다.
    // ***************1. GPU(Device) 생성
    const D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE; // 실제 GPU를 사용하겠다.

    // 여기서 생성하는 것들
    // m_device, m_context, m_swapChain,
    // m_renderTargetView, m_screenViewport, m_rasterizerSate

    // m_device와 m_context 생성

    UINT createDeviceFlags = 0;
    // 디버그를 사용할껀지 확인
#if defined(DEBUG) || defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;

    const D3D_FEATURE_LEVEL featureLevels[2] = {
        D3D_FEATURE_LEVEL_11_0, // 더 높은 버전이 먼저 오도록 설정
        D3D_FEATURE_LEVEL_9_3};
    D3D_FEATURE_LEVEL featureLevel;

    if (FAILED(D3D11CreateDevice(
            nullptr,                  // Specify nullptr to use the default adapter.
            driverType,               // Create a device using the hardware graphics driver.
            0,                        // Should be 0 unless the driver is D3D_DRIVER_TYPE_SOFTWARE.
            createDeviceFlags,        // Set debug and Direct2D compatibility flags.
            featureLevels,            // List of feature levels this app can support.
            ARRAYSIZE(featureLevels), // Size of the list above.
            D3D11_SDK_VERSION, // Always set this to D3D11_SDK_VERSION for Microsoft Store apps.
            &device,           // Returns the Direct3D device created.
            &featureLevel,     // Returns feature level of device created.
            &context           // Returns the device immediate context.
            ))) {
        cout << "D3D11CreateDevice() failed." << endl;
        return false;
    }

    /* 참고: 오류가 있을 경우 예외 발생 방법

    // MS 예제
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            // Set a breakpoint on this line to catch Win32 API errors.
            throw Platform::Exception::CreateException(hr);
        }
    }

    // Luna DX12 교재
    #ifndef ThrowIfFailed
    #define ThrowIfFailed(x)                                              \
    {                                                                     \
        HRESULT hr__ = (x);                                               \
        std::wstring wfn = AnsiToWString(__FILE__);                       \
        if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
    }
    #endif
    */

    if (featureLevel != D3D_FEATURE_LEVEL_11_0) {
        cout << "D3D Feature Level 11 unsupported." << endl;
        return false;
    }

    // 참고: Immediate vs deferred context
    // A deferred context is primarily used for multithreading and is not necessary for a
    // single-threaded application.
    // https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-intro#deferred-context

    // 4X MSAA 지원하는지 확인
    UINT numQualityLevels;
    device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &numQualityLevels);
    if (numQualityLevels <= 0) {
        cout << "MSAA not supported." << endl;
    }

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferDesc.Width = m_screenWidth;               // set the back buffer width
    sd.BufferDesc.Height = m_screenHeight;             // set the back buffer height
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // use 32-bit color
    sd.BufferCount = 2;                                // Double-buffering
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;  // how swap chain is to be used
    sd.OutputWindow = m_mainWindow;                    // the window to be used
    sd.Windowed = TRUE;                                // windowed/full-screen mode
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // allow full-screen switching
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    if (numQualityLevels > 0) {
        sd.SampleDesc.Count = 4; // how many multisamples
        sd.SampleDesc.Quality = numQualityLevels - 1;
    } else {
        sd.SampleDesc.Count = 1; // how many multisamples
        sd.SampleDesc.Quality = 0;
    }

    if (FAILED(device.As(&m_device))) {
        cout << "device.AS() failed." << endl;
        return false;
    }

    if (FAILED(context.As(&m_context))) {
        cout << "context.As() failed." << endl;
        return false;
    }

    // 참고: IDXGIFactory를 이용한 CreateSwapChain()
    /*
    ComPtr<IDXGIDevice3> dxgiDevice;
    m_device.As(&dxgiDevice);

    ComPtr<IDXGIAdapter> dxgiAdapter;
    dxgiDevice->GetAdapter(&dxgiAdapter);

     ComPtr<IDXGIFactory> dxgiFactory;
     dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));

     ComPtr<IDXGISwapChain> swapChain;
     dxgiFactory->CreateSwapChain(m_device.Get(), &sd, &swapChain);

     swapChain.As(&m_swapChain);
     */

    // 참고: IDXGIFactory4를 이용한 CreateSwapChainForHwnd()
    /*
    ComPtr<IDXGIFactory4> dxgiFactory;
    dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};
    swapChainDesc.Width = lround(m_screenWidth); // Match the size of the window.
    swapChainDesc.Height = lround(m_screenHeight);
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // This is the most common swap chain format.
    swapChainDesc.Stereo = false;
    swapChainDesc.SampleDesc.Count = 1; // Don't use multi-sampling.
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2; // Use double-buffering to minimize latency.
    swapChainDesc.SwapEffect =
        DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // All Microsoft Store apps must use this SwapEffect.
    swapChainDesc.Flags = 0;
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    ComPtr<IDXGISwapChain1> swapChain;
    dxgiFactory->CreateSwapChainForHwnd(m_device.Get(), m_mainWindow, &swapChainDesc, nullptr,
    nullptr, swapChain.GetAddressOf());
    */

    if (FAILED(D3D11CreateDeviceAndSwapChain(0, // Default adapter
                                             driverType,
                                             0, // No software device
                                             createDeviceFlags, featureLevels, 1, D3D11_SDK_VERSION,
                                             &sd, &m_swapChain, &m_device, &featureLevel,
                                             &m_context))) {
        cout << "D3D11CreateDeviceAndSwapChain() failed." << endl;
        return false;
    }

    // CreateRenderTarget

    CreateBuffers();
    // Set the viewport
    ZeroMemory(&m_screenViewport, sizeof(D3D11_VIEWPORT));
    m_screenViewport.TopLeftX = 0;
    m_screenViewport.TopLeftY = 0;
    m_screenViewport.Width = float(m_screenWidth);
    m_screenViewport.Height = float(m_screenHeight);
    // m_screenViewport.Width = static_cast<float>(m_screenHeight);
    m_screenViewport.MinDepth = 0.0f;
    m_screenViewport.MaxDepth = 1.0f; // Note: important for depth buffering

    m_context->RSSetViewports(1, &m_screenViewport);

    //  -> Create a rasterizer state
    D3D11_RASTERIZER_DESC rastDesc;
    ZeroMemory(&rastDesc, sizeof(D3D11_RASTERIZER_DESC)); // Need this
    rastDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
    rastDesc.CullMode = D3D11_CULL_MODE::D3D11_CULL_BACK;
    rastDesc.FrontCounterClockwise = false;

    ThrowIfFailed(m_device->CreateRasterizerState(&rastDesc, &m_solidRS));

    rastDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_WIREFRAME;

    ThrowIfFailed(m_device->CreateRasterizerState(&rastDesc, &m_wireRS));

    // 거울용
    rastDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;

    rastDesc.FrontCounterClockwise = true; // CCW! 반사용
    rastDesc.DepthClipEnable = true;
    ThrowIfFailed(m_device->CreateRasterizerState(&rastDesc, m_solidCCWRS.GetAddressOf()));

    rastDesc.FillMode = D3D11_FILL_WIREFRAME;
    ThrowIfFailed(m_device->CreateRasterizerState(&rastDesc, m_wireCCWRS.GetAddressOf()));

    // 거울용 ConstBuffer 생성
    D3D11Utils::CreateConstBuffer(m_device, m_eyeViewProjConstData, m_eyeViewProjConstBuffer);
    D3D11Utils::CreateConstBuffer(m_device, m_mirrorEyeViewProjConstData,
                                  m_mirrorEyeViewProjConstBuffer);
    // <-Create a rasterizer state

    // m_drawDSS: 지금까지 사용해온 기본 DSS
    D3D11_DEPTH_STENCIL_DESC dsDesc;
    ZeroMemory(&dsDesc, sizeof(dsDesc));
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    dsDesc.StencilEnable = false; // Stencil 불필요
    dsDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
    dsDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    // 앞면에 대해서 어떻게 작동할지 설정
    dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    // 뒷면에 대해 어떻게 작동할지 설정 (뒷면도 그릴 경우)
    dsDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
    dsDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    ThrowIfFailed(m_device->CreateDepthStencilState(&dsDesc, m_drawDSS.GetAddressOf()));

    // Stencil에 1로 표기해주는 DSS
    dsDesc.DepthEnable = true; // 이미 그려진 물체 유지
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    dsDesc.StencilEnable = true;    // Stencil 필수
    dsDesc.StencilReadMask = 0xFF;  // 모든 비트 다 사용
    dsDesc.StencilWriteMask = 0xFF; // 모든 비트 다 사용
    // 앞면에 대해서 어떻게 작동할지 설정
    dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
    dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    ThrowIfFailed(m_device->CreateDepthStencilState(&dsDesc, m_maskDSS.GetAddressOf()));

    // Stencil에 1로 표기된 경우에"만" 그리는 DSS
    // DepthBuffer는 초기화된 상태로 가정
    // D3D11_COMPARISON_EQUAL 이미 1로 표기된 경우에만 그리기
    // OMSetDepthStencilState(..., 1); <- 여기의 1
    dsDesc.DepthEnable = true;   // 거울 속을 다시 그릴때 필요
    dsDesc.StencilEnable = true; // Stencil 사용
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; // <- 주의
    dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;

    ThrowIfFailed(m_device->CreateDepthStencilState(&dsDesc, m_drawMaskedDSS.GetAddressOf()));

    /* "이미 그려져있는 화면"과 어떻게 섞을지를 결정
     * Dest: 이미 그려져 있는 값들을 의미
     * Src: 픽셀 쉐이더가 계산한 값들을 의미 (여기서는 마지막 거울)
     */
    D3D11_BLEND_DESC mirrorBlendDesc;
    ZeroMemory(&mirrorBlendDesc, sizeof(mirrorBlendDesc));
    mirrorBlendDesc.AlphaToCoverageEnable = true; // MSAA
    mirrorBlendDesc.IndependentBlendEnable = false;
    // 개별 RenderTarget에 대해서 설정 (최대 8개)
    mirrorBlendDesc.RenderTarget[0].BlendEnable = true;
    mirrorBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_INV_BLEND_FACTOR;
    mirrorBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_BLEND_FACTOR;
    mirrorBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

    mirrorBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    mirrorBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    mirrorBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

    // 필요하면 RGBA 각각에 대해서도 조절 가능
    mirrorBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ThrowIfFailed(m_device->CreateBlendState(&mirrorBlendDesc, m_mirrorBS.GetAddressOf()));
    return true;
}

bool EngineBase::InitGUI() {

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.DisplaySize = ImVec2(float(m_screenWidth), float(m_screenHeight));
    ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    if (!ImGui_ImplDX11_Init(m_device.Get(), m_context.Get())) {
        return false;
    }

    if (!ImGui_ImplWin32_Init(m_mainWindow)) {
        return false;
    }

    return true;
}

// 참고: How To: Compile a Shader
// https://docs.microsoft.com/en-us/windows/win32/direct3d11/how-to--compile-a-shader

// 참고: 앞에 L이 붙은 문자열은 wide character로 이루어진 문자열을
// 의미합니다.
// String and character literals (C++)
// https://learn.microsoft.com/en-us/cpp/cpp/string-and-character-literals-cpp?view=msvc-170
void EngineBase::CreateBuffers() {

    // 레스터화 -> float/depthBuffer(MSAA) -> resolved -> backBuffer

    // BackBuffer는 화면으로 최종 출력되기 때문에  RTV만 필요하고 SRV는 불필요
    ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())));
    ThrowIfFailed(
        m_device->CreateRenderTargetView(backBuffer.Get(), NULL, m_backBufferRTV.GetAddressOf()));

    // FLOAT MSAA RenderTargetView/ShaderResourceView
    ThrowIfFailed(m_device->CheckMultisampleQualityLevels(DXGI_FORMAT_R16G16B16A16_FLOAT, 4,
                                                          &m_numQualityLevels));

    D3D11_TEXTURE2D_DESC desc;
    backBuffer->GetDesc(&desc);
    desc.MipLevels = desc.ArraySize = 1;
    desc.BindFlags =
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE; // 그리기 + 읽기 둘 다 가능
    desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;              // HDR 포맷
    desc.Usage = D3D11_USAGE_DEFAULT;                          // 스테이징 텍스춰로부터 복사 가능
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

    D3D11Utils::CreateDepthBuffer(m_device, m_screenWidth, m_screenHeight,
                                  UINT(m_useMSAA ? m_numQualityLevels : 0), m_depthStencilView);

    // FLOAT MSAA를 Relsolve해서 저장할 SRV/RTV
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    ThrowIfFailed(m_device->CreateTexture2D(&desc, NULL, m_resolvedBuffer.GetAddressOf()));
    ThrowIfFailed(m_device->CreateShaderResourceView(m_resolvedBuffer.Get(), NULL,
                                                     m_resolvedSRV.GetAddressOf()));
    ThrowIfFailed(m_device->CreateRenderTargetView(m_resolvedBuffer.Get(), NULL,
                                                   m_resolvedRTV.GetAddressOf()));

    m_postProcess.Initialize(m_device, m_context, {m_resolvedSRV}, {m_backBufferRTV}, m_screenWidth,
                             m_screenHeight, 4);
}

void EngineBase::UpdateEyeViewProjBuffers(const Vector3 &eyeWorld, const Matrix &viewRow,
                                          const Matrix &projRow, const Matrix &refl) {
    m_eyeViewProjConstData.eyeWorld = eyeWorld;
    m_eyeViewProjConstData.viewProj = (viewRow * projRow).Transpose();

    m_mirrorEyeViewProjConstData.eyeWorld = eyeWorld;
    m_mirrorEyeViewProjConstData.viewProj = (refl * viewRow * projRow).Transpose();

    D3D11Utils::UpdateBuffer(m_device, m_context, m_eyeViewProjConstData, m_eyeViewProjConstBuffer);
    D3D11Utils::UpdateBuffer(m_device, m_context, m_mirrorEyeViewProjConstData,
                             m_mirrorEyeViewProjConstBuffer);
}
} // namespace hlab