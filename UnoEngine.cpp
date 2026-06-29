#include "UnoEngine.h"
#include "GeometryGenerator.h"

#include <tuple>
#include <vector>

namespace hlab {

using namespace std;


UnoEngine::UnoEngine() : EngineBase() {}

bool UnoEngine::Initialize() {

    if (!EngineBase::Initialize())
        return false;
    m_cubeMapping.Initialize(m_device, L"./Assets/Textures/Cubemaps/HDRI/indoorEnvHDR.dds",
                             L"./Assets/Textures/Cubemaps/HDRI/indoorSpecularHDR.dds",
                             L"./Assets/Textures/Cubemaps/HDRI/indoorDiffuseHDR.dds",
                             L"./Assets/Textures/Cubemaps/HDRI/indoorBrdf.dds");
     
    // Main Sphere
    {
        Vector3 center(0.0f, 0.5f, 1.0f);
        float radius = 0.4f;
        MeshData sphere = GeometryGenerator::MakeSphere(radius, 100, 100, {1.0f, 1.0f});

        // 여러가지 텍스춰들!
        sphere.albedoTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                       "grey_porous_rock_40_56_diffuse.jpg";

        sphere.normalTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                       "grey_porous_rock_40_56_normal.jpg";

        sphere.heightTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                       "grey_porous_rock_40_56_height.jpg";
        m_mainSphere.m_basicPixelConstantData.reverseNormalMapY = true;

        sphere.aoTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                   "grey_porous_rock_40_56_ao.jpg";

        m_mainSphere.Initialize(m_device, m_context, {sphere});
        m_mainSphere.m_diffuseResView = m_cubeMapping.m_diffuseResView;
        m_mainSphere.m_specularResView = m_cubeMapping.m_specularResView;
        m_mainSphere.UpdateModelWorld(Matrix::CreateTranslation(center));
        m_mainSphere.m_basicPixelConstantData.useTexture = true;

        /*m_mainSphere.m_basicPixelConstantData.material.diffuse = Vector3(1.0f);
        m_mainSphere.m_basicPixelConstantData.material.specular = Vector3(0.0f);*/
        m_mainSphere.m_basicPixelConstantData.indexColor = Vector4(1.0f, 0.0, 0.0, 0.0);
        m_mainSphere.UpdateConstantBuffers(m_device, m_context);

        // 동일한 크기와 위치에 BoundingSphere 만들기
        // m_mainBoundingSphere = BoundingSphere(center, radius);
    }
    return true;
}

void UnoEngine::Update(float dt) {
    // dt는 이전 프레임과 다음 프레임의 시간차
    
    // 카메라 이동
    m_camera.UpdateKeyboard(dt, m_keyPressed);
   
    Vector3 eyeWorld = m_camera.GetEyePos();
    // Matrix reflectionRow = Matrix::CreateReflection(m_mirrorPlane);
    Matrix viewRow = m_camera.GetViewRow();
    Matrix projRow = m_camera.GetProjRow();

    // EngineBase::UpdateEyeViewProjBuffers(eyeWorld, viewRow, projRow);

    // 큐브 매핑 Constant Buffer 업데이트
    m_cubeMapping.UpdateViewProjConstBuffer(m_device, m_context, viewRow, projRow);

    using namespace DirectX;
    m_mainSphere.m_basicVertexConstantData.view = viewRow.Transpose();
    m_mainSphere.m_basicVertexConstantData.projection = projRow.Transpose();
    m_mainSphere.m_basicPixelConstantData.eyeWorld = eyeWorld;
    m_mainSphere.UpdateConstantBuffers(m_device, m_context);

}

void UnoEngine::Render() {

    // IA: Input-Assembler stage
    // VS: Vertex Shader
    // PS: Pixel Shader
    // RS: Rasterizer stage
    // OM: Output-Merger stage

    m_context->RSSetViewports(1, &m_screenViewport);

    float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

    m_context->ClearDepthStencilView(m_depthStencilView.Get(),
                                     D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // 비교: Depth Buffer를 사용하지 않는 경우
    // m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), nullptr);

    m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), m_depthStencilView.Get());

    m_context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    m_cubeMapping.Render(m_context, false);
    m_mainSphere.Render(m_context);
}

void UnoEngine::UpdateGUI() {
    ImGui::Checkbox("usePerspectiveProjection", &m_usePerspectiveProjection);
    if (ImGui::TreeNode("Post Processing")) {
        int flag = 0;
        flag += ImGui::SliderFloat("Bloom Strength",
                                   &m_postProcess.m_combineFilter.m_constData.strength, 0.0f, 1.0f);
        flag += ImGui::SliderFloat("Exposure", &m_postProcess.m_combineFilter.m_constData.option1,
                                   0.0f, 10.0f);
        flag += ImGui::SliderFloat("Gamma", &m_postProcess.m_combineFilter.m_constData.option2,
                                   0.1f, 5.0f);
        // 편의상 사용자 입력이 인식되면 바로 GPU 버퍼를 업데이트
        if (flag) {
            m_postProcess.m_combineFilter.UpdateConstantBuffers(m_device, m_context);
        }
        ImGui::TreePop();
    }
}

} // namespace hlab
