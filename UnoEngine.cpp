#include "UnoEngine.h"
#include "GeometryGenerator.h"

#include <tuple>
#include <vector>

namespace hlab {

using namespace std;
using namespace DirectX;
UnoEngine::UnoEngine() : EngineBase() {}

bool UnoEngine::Initialize() {

    if (!EngineBase::Initialize())
        return false;
    m_cubeMapping.Initialize(m_device, L"./Assets/Textures/Cubemaps/HDRI/indoorEnvHDR.dds",
                             L"./Assets/Textures/Cubemaps/HDRI/indoorSpecularHDR.dds",
                             L"./Assets/Textures/Cubemaps/HDRI/indoorDiffuseHDR.dds",
                             L"./Assets/Textures/Cubemaps/HDRI/indoorBrdf.dds");
    // 거울
    {
        auto mesh = GeometryGenerator::MakeSquare(1.0f);
        m_mirror = make_shared<BasicMeshGroup>();
        m_mirror->Initialize(m_device, m_context, vector{mesh});
        m_mirror->m_basicPixelConstData.material.albedo = Vector3(0.3f);
        m_mirror->m_basicPixelConstData.material.metallic = 0.7f;
        m_mirror->m_basicPixelConstData.material.roughness = 0.2f;

        Vector3 mirrorPos(0.5f, 0.25f, 2.0f);
        m_mirror->UpdateModelWorld(Matrix::CreateRotationY(3.141592f * 0.5f) *
                                   Matrix::CreateTranslation(mirrorPos));

        m_mirrorPlane = DirectX::SimpleMath::Plane(mirrorPos, Vector3(-1.0f, 0.0f, 0.0f));
        m_mirror->UpdateConstantBuffers(m_device, m_context);
    }

    // Main Sphere
    {
        Vector3 center(0.0f, 0.5f, 1.0f);
        float radius = 0.4f;
        MeshData sphere = GeometryGenerator::MakeSphere(radius, 100, 100, {1.0f, 1.0f});
        m_mainSphere = make_shared<BasicMeshGroup>(); // ← 이 줄 추가!
        // PBR textures
        sphere.albedoTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                       "grey_porous_rock_40_56_diffuse.jpg";
        sphere.normalTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                       "grey_porous_rock_40_56_normal.jpg";

        sphere.heightTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                       "grey_porous_rock_40_56_height.jpg";
        m_mainSphere->m_basicPixelConstData.invertNormalMapY = true;

        sphere.aoTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                   "grey_porous_rock_40_56_ao.jpg";

        sphere.metallicTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                         "grey_porous_rock_40_56_metallic.jpg";

        sphere.roughnessTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                          "grey_porous_rock_40_56_roughness.jpg";

        m_mainSphere->Initialize(m_device, m_context, {sphere});
        m_mainSphere->m_irradianceSRV = m_cubeMapping.m_irradianceSRV;
        m_mainSphere->m_specularSRV = m_cubeMapping.m_specularSRV;
        m_mainSphere->m_brdfSRV = m_cubeMapping.m_brdfSRV;
        m_mainSphere->UpdateModelWorld(Matrix::CreateTranslation(center));
        // m_mainSphere->m_basicPixelConstData.useAlbedoMap = true;

        /*m_mainSphere->m_basicPixelConstantData.material.diffuse = Vector3(1.0f);
        m_mainSphere->m_basicPixelConstantData.material.specular = Vector3(0.0f);*/
        // m_mainSphere->m_basicPixelConstData.indexColor = Vector4(1.0f, 0.0, 0.0, 0.0);
        m_mainSphere->UpdateConstantBuffers(m_device, m_context);

        // 동일한 크기와 위치에 BoundingSphere 만들기
        // m_mainBoundingSphere = BoundingSphere(center, radius);

        m_basicList.push_back(m_mainSphere);
    }

    // 기존에 m_basicList에 넣던 물체들 (Sphere 등) 추가

    return true;
}

void UnoEngine::Update(float dt) {
    // dt는 이전 프레임과 다음 프레임의 시간차

    // 카메라 이동
    m_camera.UpdateKeyboard(dt, m_keyPressed);

    Vector3 eyeWorld = m_camera.GetEyePos();
    Matrix reflectionRow = Matrix::CreateReflection(m_mirrorPlane);
    Matrix viewRow = m_camera.GetViewRow();
    Matrix projRow = m_camera.GetProjRow();

    EngineBase::UpdateEyeViewProjBuffers(eyeWorld, viewRow, projRow, reflectionRow);
    // 큐브 매핑 Constant Buffer 업데이트
    m_cubeMapping.UpdateViewProjConstBuffer(m_device, m_context, viewRow, projRow, reflectionRow);
    // 포인트 라이트 효과
    Light pointLight;
    pointLight.position = m_lightPosition;
    pointLight.strength = Vector3(1.0f); // Strength
    pointLight.fallOffEnd = 20.0f;

    m_mainSphere->m_basicPixelConstData.lights[1] = pointLight;

    m_mainSphere->UpdateConstantBuffers(m_device, m_context);

    // 거울은 따로 처리
    m_mirror->m_basicPixelConstData.lights[1] = pointLight;
    m_mirror->UpdateConstantBuffers(m_device, m_context);
}

void UnoEngine::Render() {

    // IA: Input-Assembler stage
    // VS: Vertex Shader
    // PS: Pixel Shader
    // RS: Rasterizer stage
    // OM: Output-Merger stage

    m_context->RSSetViewports(1, &m_screenViewport);

    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    vector<ID3D11RenderTargetView *> renderTargetViews = {m_floatRTV.Get()};
    for (size_t i = 0; i < renderTargetViews.size(); i++) {
        m_context->ClearRenderTargetView(renderTargetViews[i], clearColor);
    }
    m_context->ClearDepthStencilView(m_depthStencilView.Get(),
                                     D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    m_context->OMSetRenderTargets(UINT(renderTargetViews.size()), renderTargetViews.data(),
                                  m_depthStencilView.Get());
    m_context->OMSetDepthStencilState(m_drawDSS.Get(), 0);

    /* 거울 1. 원래 대로 한 번 그림 */

    // 기본 BlendState 사용
    m_context->OMSetBlendState(NULL, NULL, 0xffffffff);

    m_context->ClearDepthStencilView(m_depthStencilView.Get(),
                                     D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 1);

    m_context->OMSetDepthStencilState(m_drawDSS.Get(), 1);

    m_context->RSSetState(m_isDrawAsWire ? m_wireRS.Get() : m_solidRS.Get());

    for (auto &i : m_basicList) {
        i->Render(m_context, EngineBase::m_eyeViewProjConstBuffer, m_useEnv);
    }

    // if (m_useEnv) { TODO: 환경맵을 그릴지 말지 결정하는 옵션 필요
    m_cubeMapping.Render(m_context, false);

    /* 거울 2. 거울 위치만 StencilBuffer에 1로 표기 */

    // STENCIL만 클리어
    // 거울을 가리는 물체가 있을 수도 있어서 Depth는 CLEAR 안함
    // 앞 단계의 m_drawDSS에서 모두 KEEP을 사용했기 때문에
    // Stencil도 CLEAR 불필요
     m_context->ClearDepthStencilView(m_depthStencilView.Get(),
                                     D3D11_CLEAR_STENCIL, 1.0f, 0);

    // 두 번째 UINT StencilRef = 1 사용
    // ClearDepthStencilView(..., 0)에서는 다른 숫자 0 사용
    // TODO:
    m_context->OMSetDepthStencilState(m_maskDSS.Get(), 1);

    // 거울을 그릴 때 색은 필요 없기 때문에 간단한 PS 사용 가능
    m_mirror->Render(m_context, EngineBase::m_eyeViewProjConstBuffer, m_useEnv);

    /* 거울 3. 거울 위치에 반사된 물체들을 렌더링 */
    // TODO:
    //m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    // TODO:
    m_context->OMSetDepthStencilState(m_drawMaskedDSS.Get(), 1);

    // 반사되면 삼각형 정점들의 순서(Winding)가 반대로 -> 반시계
    // TODO:
    m_context->RSSetState(m_isDrawAsWire ? m_wireCCWRS.Get() : m_solidCCWRS.Get());

    // 반사된 위치에 그려야 함
    // TODO: AppBase::m_mirrorEyeViewProjConstBuffer 사용
    for (auto &i : m_basicList) {
        i->Render(m_context, EngineBase::m_mirrorEyeViewProjConstBuffer, m_useEnv);
    }
    // 환경맵도 뒤집어서 그리기
    if (m_useEnv) {
        m_cubeMapping.Render(m_context, true);
    }
    ///* 거울 4. 거울 자체의 재질을 "Blend"로 그림 */

    //// TODO:
    const float t = 1.0f - m_mirrorAlpha;
    const float baseColor[4] = {t, t, t, 1.0f};
    m_context->OMSetBlendState(m_mirrorBS.Get(), baseColor, 0xffffffff);

    // TODO: m_context->RSSetState(...); // 다시 시계 방향
    m_context->RSSetState(m_isDrawAsWire ? m_wireRS.Get() : m_solidRS.Get());
    // TODO: 거울 그리기
    m_mirror->Render(m_context, EngineBase::m_eyeViewProjConstBuffer, m_useEnv);
    // 후처리는 Blend X
    m_context->OMSetBlendState(NULL, NULL, 0xffffffff);
    /* 이후 원래 하던 후처리 */
    m_context->ResolveSubresource(m_resolvedBuffer.Get(), 0, m_floatBuffer.Get(), 0,
                                  DXGI_FORMAT_R16G16B16A16_FLOAT);

    m_postProcess.Render(m_context);
}

void UnoEngine::UpdateGUI() {
    ImGui::SetNextItemOpen(false, ImGuiCond_Once);
    if (ImGui::TreeNode("General")) {
        ImGui::Checkbox("Wireframe", &m_isDrawAsWire);
        ImGui::TreePop();
    }
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    /*   if (ImGui::TreeNode("Skybox")) {
           ImGui::SliderFloat("Strength", &m_globalConstsCPU.strengthIBL, 0.0f, 5.0f);
           ImGui::RadioButton("Env", &m_globalConstsCPU.textureToDraw, 0);
           ImGui::SameLine();
           ImGui::RadioButton("Specular", &m_globalConstsCPU.textureToDraw, 1);
           ImGui::SameLine();
           ImGui::RadioButton("Irradiance", &m_globalConstsCPU.textureToDraw, 2);
           ImGui::SliderFloat("EnvLodBias", &m_globalConstsCPU.envLodBias, 0.0f, 10.0f);
           ImGui::TreePop();
       }*/
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
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Point Light")) {
        ImGui::SliderFloat3("Position", &m_lightPosition.x, -5.0f, 5.0f);
        ImGui::TreePop();
    }
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Material")) {

        int flag = 0;

        flag += ImGui::SliderFloat(
            "Metallic", &m_mainSphere->m_basicPixelConstData.material.metallic, 0.0f, 1.0f);
        flag += ImGui::SliderFloat(
            "Roughness", &m_mainSphere->m_basicPixelConstData.material.roughness, 0.0f, 1.0f);
        flag += ImGui::CheckboxFlags("AlbedoTexture",
                                     &m_mainSphere->m_basicPixelConstData.useAlbedoMap, 1);
        flag += ImGui::CheckboxFlags("Use NormalMapping",
                                     &m_mainSphere->m_basicPixelConstData.useNormalMap, 1);
        flag += ImGui::CheckboxFlags("Use AO", &m_mainSphere->m_basicPixelConstData.useAOMap, 1);
        flag += ImGui::CheckboxFlags("Use HeightMapping",
                                     &m_mainSphere->m_basicVertexConstData.useHeightMap, 1);
        flag += ImGui::SliderFloat("HeightScale", &m_mainSphere->m_basicVertexConstData.heightScale,
                                   0.0f, 0.1f);
        flag += ImGui::CheckboxFlags("Use MetallicMap",
                                     &m_mainSphere->m_basicPixelConstData.useMetallicMap, 1);
        flag += ImGui::CheckboxFlags("Use RoughnessMap",
                                     &m_mainSphere->m_basicPixelConstData.useRoughnessMap, 1);
        flag += ImGui::Checkbox("Draw Normals", &m_mainSphere->m_drawNormals);

        if (flag) {
            // GUI 입력이 있을 때만 할 일들 추가
        }

        ImGui::TreePop();
    }
}

} // namespace hlab
