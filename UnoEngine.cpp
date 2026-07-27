#include "UnoEngine.h"
#include "GeometryGenerator.h"
#include "GraphicsCommon.h"

#include <DirectXCollision.h>
#include <directxtk/DDSTextureLoader.h>
#include <directxtk/SimpleMath.h>
#include <tuple>
#include <vector>

namespace hlab {

using namespace std;
using namespace DirectX;
using namespace DirectX::SimpleMath;

UnoEngine::UnoEngine() : EngineBase() {}

bool UnoEngine::Initialize() {

    if (!EngineBase::Initialize())
        return false;

    EngineBase::InitCubemaps(L"./Assets/Textures/Cubemaps/HDRI/", L"indoorEnvHDR.dds",
                             L"indoorSpecularHDR.dds", L"indoorDiffuseHDR.dds", L"indoorBrdf.dds");

    // 스카이박스
    {
        MeshData skyboxMesh = GeometryGenerator::MakeBox(40.0f);
        std::reverse(skyboxMesh.indices.begin(), skyboxMesh.indices.end());
        m_skybox = make_shared<Model>(m_device, m_context, vector{skyboxMesh});
    }

    // 조명 설정
    {
        m_light.position = Vector3(0.0f, 0.5f, 1.7f);
        m_light.radiance = Vector3(5.0f);
        m_light.fallOffEnd = 20.0f;
    }

    // 거울
    {
        auto mesh = GeometryGenerator::MakeSquare(1.0f);
        m_mirror = make_shared<Model>(m_device, m_context, vector{mesh});
        m_mirror->m_materialConstsCPU.albedoFactor = Vector3(0.3f);
        m_mirror->m_materialConstsCPU.metallicFactor = 0.7f;
        m_mirror->m_materialConstsCPU.roughnessFactor = 0.2f;

        Vector3 mirrorPos(0.5f, 0.25f, 2.0f);
        m_mirror->UpdateWorldRow(Matrix::CreateRotationY(3.141592f * 0.5f) *
                                 Matrix::CreateTranslation(mirrorPos));

        m_mirrorPlane = DirectX::SimpleMath::Plane(mirrorPos, Vector3(-1.0f, 0.0f, 0.0f));
    }

    // Main Sphere
    {
        Vector3 center(0.0f, 0.5f, 1.0f);
        float radius = 0.4f;
        MeshData sphere = GeometryGenerator::MakeSphere(radius, 100, 100, {1.0f, 1.0f});

        sphere.albedoTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                       "grey_porous_rock_40_56_diffuse.jpg";
        sphere.normalTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                       "grey_porous_rock_40_56_normal.jpg";
        sphere.heightTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                       "grey_porous_rock_40_56_height.jpg";
        sphere.aoTextureFilename =
            "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/grey_porous_rock_40_56_ao.jpg";
        sphere.metallicTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                         "grey_porous_rock_40_56_metallic.jpg";
        sphere.roughnessTextureFilename = "./Assets/Textures/PBR/cgaxis_grey_porous_rock_40_56_4K/"
                                          "grey_porous_rock_40_56_roughness.jpg";

        m_mainSphere = make_shared<Model>(m_device, m_context, vector{sphere});
        m_mainSphere->m_materialConstsCPU.invertNormalMapY = true;
        m_mainSphere->UpdateWorldRow(Matrix::CreateTranslation(center));

        m_mainBoundingSphere = BoundingSphere(center, radius);

        m_basicList.push_back(m_mainSphere);
    }

    // 조명 위치 표시
    {
        MeshData sphere = GeometryGenerator::MakeSphere(0.01f, 10, 10);
        m_lightSphere = make_shared<Model>(m_device, m_context, vector{sphere});
        m_lightSphere->UpdateWorldRow(Matrix::CreateTranslation(m_light.position));
        m_lightSphere->m_materialConstsCPU.albedoFactor = Vector3(0.0f);
        m_lightSphere->m_materialConstsCPU.emissionFactor = Vector3(1.0f, 1.0f, 0.0f);

        m_basicList.push_back(m_lightSphere); // 리스트에 등록
    }

    // 커서 표시
    {
        MeshData sphere = GeometryGenerator::MakeSphere(0.01f, 10, 10);
        m_cursorSphere = make_shared<Model>(m_device, m_context, vector{sphere});
        m_cursorSphere->m_isVisible = false;
        m_cursorSphere->m_materialConstsCPU.albedoFactor = Vector3(0.0f);
        m_cursorSphere->m_materialConstsCPU.emissionFactor = Vector3(0.0f, 1.0f, 0.0f);

        m_basicList.push_back(m_cursorSphere);
    }

    return true;
}
void UnoEngine::Update(float dt) {
    m_camera.UpdateKeyboard(dt, m_keyPressed);

    Vector3 eyeWorld = m_camera.GetEyePos();
    Matrix reflectionRow = Matrix::CreateReflection(m_mirrorPlane);
    Matrix viewRow = m_camera.GetViewRow();
    Matrix projRow = m_camera.GetProjRow();

    // 조명 설정 (GlobalConstants에 딱 한 번만!)
    m_globalConstsCPU.lights[1] = m_light;

    EngineBase::UpdateGlobalConstants(eyeWorld, viewRow, projRow, reflectionRow);

    // 거울은 따로 처리
    m_mirror->UpdateConstantBuffers(m_device, m_context);

    // 조명의 위치 반영
    m_lightSphere->UpdateWorldRow(Matrix::CreateTranslation(m_light.position));

    // ---- Picking 로직 (동일한 알고리즘, Model API로 교체) ----
    static float prevRatio = 0.0f;
    static Vector3 prevPos(0.0f);
    static Vector3 prevVector(0.0f);
    Quaternion q = Quaternion::CreateFromAxisAngle(Vector3(1.0f, 0.0f, 0.0f), 0.0f);
    Vector3 dragTranslation(0.0f);

    if (m_leftButton) {
        Vector3 cursorNdcNear = Vector3(m_cursorNdcX, m_cursorNdcY, 0.0f);
        Vector3 cursorNdcFar = Vector3(m_cursorNdcX, m_cursorNdcY, 1.0f);
        Matrix inverseProjView = (viewRow * projRow).Invert();
        Vector3 cursorWorldNear = Vector3::Transform(cursorNdcNear, inverseProjView);
        Vector3 cursorWorldFar = Vector3::Transform(cursorNdcFar, inverseProjView);
        Vector3 dir = cursorWorldFar - cursorWorldNear;
        dir.Normalize();

        SimpleMath::Ray curRay = SimpleMath::Ray(cursorWorldNear, dir);
        float dist = 0.0f;
        m_selected = curRay.Intersects(m_mainBoundingSphere, dist);
        cout << "click Cursor NDC in Uno: (" << m_cursorNdcX << ", " << m_cursorNdcY << ")" << endl;
        if (m_selected) {
            Vector3 pickPoint = cursorWorldNear + dist * dir;

            m_cursorSphere->UpdateWorldRow(Matrix::CreateTranslation(pickPoint));
            m_cursorSphere->m_isVisible = true;

            if (m_dragStartFlag) {
                m_dragStartFlag = false;
                prevVector = pickPoint - m_mainBoundingSphere.Center;
                prevVector.Normalize();
            } else {
                Vector3 currentVector = pickPoint - m_mainBoundingSphere.Center;
                currentVector.Normalize();
                float dotValue = std::clamp(prevVector.Dot(currentVector), -1.0f, 1.0f);
                float theta = acos(dotValue);
                if (theta > 3.141592f / 180.0f * 3.0f) {
                    Vector3 axis = prevVector.Cross(currentVector);
                    axis.Normalize();
                    q = SimpleMath::Quaternion::CreateFromAxisAngle(axis, theta);
                    prevVector = currentVector;
                }
            }
        } else {
            m_cursorSphere->m_isVisible = false;
        }
    }

    if (m_rightButton) {
        Vector3 cursorNdcNear = Vector3(m_cursorNdcX, m_cursorNdcY, 0.0f);
        Vector3 cursorNdcFar = Vector3(m_cursorNdcX, m_cursorNdcY, 1.0f);
        Matrix inverseProjView = (viewRow * projRow).Invert();
        Vector3 cursorWorldNear = Vector3::Transform(cursorNdcNear, inverseProjView);
        Vector3 cursorWorldFar = Vector3::Transform(cursorNdcFar, inverseProjView);
        Vector3 dir = cursorWorldFar - cursorWorldNear;
        dir.Normalize();

        SimpleMath::Ray curRay = SimpleMath::Ray(cursorWorldNear, dir);
        float dist = 0.0f;
        m_selected = curRay.Intersects(m_mainBoundingSphere, dist);

        if (m_selected) {
            Vector3 pickPoint = cursorWorldNear + dist * dir;

            m_cursorSphere->m_isVisible = true;
            m_cursorSphere->UpdateWorldRow(Matrix::CreateTranslation(pickPoint));

            if (m_dragStartFlag) {
                m_dragStartFlag = false;
                prevRatio = dist / (cursorWorldFar - cursorWorldNear).Length();
                prevPos = pickPoint;
            } else {
                Vector3 newPos = cursorWorldNear + prevRatio * (cursorWorldFar - cursorWorldNear);
                if ((newPos - prevPos).Length() > 1e-3) {
                    dragTranslation = newPos - prevPos;
                    prevPos = newPos;
                }
            }
        } else {
            m_cursorSphere->m_isVisible = false;
        }
    }

    if (!m_leftButton && !m_rightButton) {
        m_cursorSphere->m_isVisible = false;
    }

    Vector3 translation = m_mainSphere->m_worldRow.Translation();
    m_mainSphere->m_worldRow.Translation(Vector3(0.0f));
    m_mainSphere->UpdateWorldRow(m_mainSphere->m_worldRow * Matrix::CreateFromQuaternion(q) *
                                 Matrix::CreateTranslation(dragTranslation + translation));
    m_mainBoundingSphere.Center = m_mainSphere->m_worldRow.Translation();

    // 모든 오브젝트 constant buffer 갱신 (조명은 이미 GlobalConstants에 있음)
    for (auto &i : m_basicList) {
        i->UpdateConstantBuffers(m_device, m_context);
    }
}
void UnoEngine::Render() {
    m_context->RSSetViewports(1, &m_screenViewport);

    // 공용 샘플러 바인딩 (Common.hlsli의 s0, s1)
    m_context->VSSetSamplers(0, UINT(Graphics::sampleStates.size()), Graphics::sampleStates.data());
    m_context->PSSetSamplers(0, UINT(Graphics::sampleStates.size()), Graphics::sampleStates.data());

    // 공용 IBL 텍스처 바인딩 (Common.hlsli의 t10~13)
    vector<ID3D11ShaderResourceView *> commonSRVs = {m_envSRV.Get(), m_specularSRV.Get(),
                                                     m_irradianceSRV.Get(), m_brdfSRV.Get()};
    m_context->PSSetShaderResources(10, UINT(commonSRVs.size()), commonSRVs.data());

    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    m_context->ClearRenderTargetView(m_floatRTV.Get(), clearColor);
    m_context->ClearDepthStencilView(m_depthStencilView.Get(),
                                     D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    m_context->OMSetRenderTargets(1, m_floatRTV.GetAddressOf(), m_depthStencilView.Get());

    /* 1단계: 거울 제외 일반 렌더링 */
    EngineBase::SetPipelineState(m_isDrawAsWire ? Graphics::defaultWirePSO
                                                : Graphics::defaultSolidPSO);
    EngineBase::SetGlobalConsts(m_globalConstsGPU);

    for (auto &i : m_basicList) {
        i->Render(m_context);
    }

    EngineBase::SetPipelineState(m_isDrawAsWire ? Graphics::skyboxWirePSO
                                                : Graphics::skyboxSolidPSO);
    m_skybox->Render(m_context);

    /* 2단계: 거울 위치만 Stencil에 표시 (Stencil만 새로 클리어, Depth는 유지) */
    m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_STENCIL, 1.0f, 0);
    EngineBase::SetPipelineState(Graphics::stencilMaskPSO);
    m_mirror->Render(m_context);

    /* 3단계: 반사된 물체 렌더링 (Depth는 지우지 않음!) */
    EngineBase::SetPipelineState(m_isDrawAsWire ? Graphics::reflectWirePSO
                                                : Graphics::reflectSolidPSO);
    EngineBase::SetGlobalConsts(m_reflectGlobalConstsGPU);

    for (auto &i : m_basicList) {
        i->Render(m_context);
    }

    EngineBase::SetPipelineState(m_isDrawAsWire ? Graphics::reflectSkyboxWirePSO
                                                : Graphics::reflectSkyboxSolidPSO);
    m_skybox->Render(m_context);

    /* 4단계: 거울 자체를 블렌딩으로 그림 */
    EngineBase::SetPipelineState(m_isDrawAsWire ? Graphics::mirrorBlendWirePSO
                                                : Graphics::mirrorBlendSolidPSO);
    const float blendColor[4] = {m_mirrorAlpha, m_mirrorAlpha, m_mirrorAlpha, 1.0f};
    if (m_isDrawAsWire)
        Graphics::mirrorBlendWirePSO.SetBlendFactor(blendColor);
    else
        Graphics::mirrorBlendSolidPSO.SetBlendFactor(blendColor);
    EngineBase::SetPipelineState(m_isDrawAsWire ? Graphics::mirrorBlendWirePSO
                                                : Graphics::mirrorBlendSolidPSO);
    EngineBase::SetGlobalConsts(m_globalConstsGPU);
    m_mirror->Render(m_context);

    /* 후처리 */
    m_context->ResolveSubresource(m_resolvedBuffer.Get(), 0, m_floatBuffer.Get(), 0,
                                  DXGI_FORMAT_R16G16B16A16_FLOAT);
    EngineBase::SetPipelineState(Graphics::postProcessingPSO);
    m_postProcess.Render(m_context);
}
void UnoEngine::UpdateGUI() {

    ImGui::SetNextItemOpen(false, ImGuiCond_Once);
    if (ImGui::TreeNode("General")) {

        ImGui::Checkbox("Wireframe", &m_isDrawAsWire);
        if (ImGui::Checkbox("MSAA ON", &m_useMSAA)) {
            CreateBuffers();
        }
        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Env Map")) {
        ImGui::SliderFloat("Strength", &m_globalConstsCPU.strengthIBL, 0.0f, 5.0f);
        ImGui::RadioButton("Env", &m_globalConstsCPU.textureToDraw, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Specular", &m_globalConstsCPU.textureToDraw, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Irradiance", &m_globalConstsCPU.textureToDraw, 2);
        ImGui::SliderFloat("EnvLodBias", &m_globalConstsCPU.envLodBias, 0.0f, 10.0f);
        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(false, ImGuiCond_Once);
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
    if (ImGui::TreeNode("Mirror")) {

        ImGui::SliderFloat("Alpha", &m_mirrorAlpha, 0.0f, 1.0f);
        const float blendColor[4] = {m_mirrorAlpha, m_mirrorAlpha, m_mirrorAlpha, 1.0f};
        if (m_isDrawAsWire)
            Graphics::mirrorBlendWirePSO.SetBlendFactor(blendColor);
        else
            Graphics::mirrorBlendSolidPSO.SetBlendFactor(blendColor);

        ImGui::SliderFloat("Metallic", &m_mirror->m_materialConstsCPU.metallicFactor, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &m_mirror->m_materialConstsCPU.roughnessFactor, 0.0f, 1.0f);

        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Point Light")) {
        ImGui::SliderFloat3("Position", &m_light.position.x, -5.0f, 5.0f);
        ImGui::TreePop();
    }

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (ImGui::TreeNode("Material")) {
        ImGui::SliderFloat("LodBias", &m_globalConstsCPU.lodBias, 0.0f, 10.0f);

        int flag = 0;

        flag += ImGui::SliderFloat("Metallic", &m_mainSphere->m_materialConstsCPU.metallicFactor,
                                   0.0f, 1.0f);
        flag += ImGui::SliderFloat("Roughness", &m_mainSphere->m_materialConstsCPU.roughnessFactor,
                                   0.0f, 1.0f);
        flag += ImGui::CheckboxFlags("AlbedoTexture",
                                     &m_mainSphere->m_materialConstsCPU.useAlbedoMap, 1);
        flag += ImGui::CheckboxFlags("EmissiveTexture",
                                     &m_mainSphere->m_materialConstsCPU.useEmissiveMap, 1);
        flag += ImGui::CheckboxFlags("Use NormalMapping",
                                     &m_mainSphere->m_materialConstsCPU.useNormalMap, 1);
        flag += ImGui::CheckboxFlags("Use AO", &m_mainSphere->m_materialConstsCPU.useAOMap, 1);
        flag += ImGui::CheckboxFlags("Use HeightMapping",
                                     &m_mainSphere->m_meshConstsCPU.useHeightMap, 1);
        flag += ImGui::SliderFloat("HeightScale", &m_mainSphere->m_meshConstsCPU.heightScale, 0.0f,
                                   0.1f);
        flag += ImGui::CheckboxFlags("Use MetallicMap",
                                     &m_mainSphere->m_materialConstsCPU.useMetallicMap, 1);
        flag += ImGui::CheckboxFlags("Use RoughnessMap",
                                     &m_mainSphere->m_materialConstsCPU.useRoughnessMap, 1);

        if (flag) {
            m_mainSphere->UpdateConstantBuffers(m_device, m_context);
        }

        ImGui::Checkbox("Draw Normals", &m_mainSphere->m_drawNormals);

        ImGui::TreePop();
    }
}
} // namespace hlab