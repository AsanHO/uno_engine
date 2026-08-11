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

    // 바닥(거울)
    {
        auto mesh = GeometryGenerator::MakeSquare(5.0);
        // mesh.albedoTextureFilename =
        //     "../Assets/Textures/blender_uv_grid_2k.png";
        m_ground = make_shared<Model>(m_device, m_context, vector{mesh});
        m_ground->m_materialConstsCPU.albedoFactor = Vector3(0.1f);
        m_ground->m_materialConstsCPU.emissionFactor = Vector3(0.0f);
        m_ground->m_materialConstsCPU.metallicFactor = 0.5f;
        m_ground->m_materialConstsCPU.roughnessFactor = 0.3f;

        Vector3 position = Vector3(0.0f, -0.5f, 2.0f);
        m_ground->UpdateWorldRow(Matrix::CreateRotationX(3.141592f * 0.5f) *
                                 Matrix::CreateTranslation(position));

        m_mirrorPlane = SimpleMath::Plane(position, Vector3(0.0f, 1.0f, 0.0f));
        m_mirror = m_ground; // 바닥에 거울처럼 반사 구현

        // m_basicList.push_back(m_ground); // 거울은 리스트에 등록 X
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

        m_mainPhysicsBody.position = center;
        m_mainPhysicsBody.radius = radius;
        m_mainPhysicsBody.velocity = Vector3(0.0f);
        m_mainPhysicsBody.restitution = 0.8f;

        m_basicList.push_back(m_mainSphere);
    }
    // 추가 물체1
    {
        MeshData mesh = GeometryGenerator::MakeSphere(0.2f, 200, 200);
        Vector3 center(0.5f, 0.5f, 2.0f);
        auto m_obj = make_shared<Model>(m_device, m_context, vector{mesh});
        m_obj->UpdateWorldRow(Matrix::CreateTranslation(center));
        m_obj->m_materialConstsCPU.albedoFactor = Vector3(0.1f, 0.1f, 1.0f);
        m_obj->m_materialConstsCPU.roughnessFactor = 0.2f;
        m_obj->m_materialConstsCPU.metallicFactor = 0.6f;
        m_obj->m_materialConstsCPU.emissionFactor = Vector3(0.0f);
        m_obj->UpdateConstantBuffers(m_device, m_context);

        m_basicList.push_back(m_obj);
    }

    // 물리 데모 (2단계: 구 여러 개, 중력 + 바닥 충돌 + 구-구 충돌)
    {
        const Vector3 palette[5] = {
            Vector3(0.85f, 0.2f, 0.2f),  // 빨강
            Vector3(0.2f, 0.55f, 0.85f), // 파랑
            Vector3(0.25f, 0.75f, 0.35f),// 초록
            Vector3(0.85f, 0.65f, 0.15f),// 주황
            Vector3(0.6f, 0.3f, 0.75f),  // 보라
        };

        const int cols = 5;
        for (int i = 0; i < NUM_PHYSICS_SPHERES; i++) {
            const int row = i / cols;
            const int col = i % cols;

            auto &body = m_physicsBodies[i];
            body.radius = 0.13f + 0.02f * float(i % 3);
            body.position = Vector3(-1.0f + col * 0.5f, 1.5f + float(i % 4) * 0.6f,
                                    1.4f + row * 0.5f);
            body.velocity = Vector3(0.0f);
            body.restitution = 0.6f;

            MeshData mesh = GeometryGenerator::MakeSphere(body.radius, 40, 40);
            auto &model = m_physicsSphereModels[i];
            model = make_shared<Model>(m_device, m_context, vector{mesh});
            model->UpdateWorldRow(Matrix::CreateTranslation(body.position));
            model->m_materialConstsCPU.albedoFactor = palette[i % 5];
            model->m_materialConstsCPU.roughnessFactor = 0.3f;
            model->m_materialConstsCPU.metallicFactor = 0.1f;
            model->m_materialConstsCPU.emissionFactor = Vector3(0.0f);

            m_basicList.push_back(model);
        }
    }

    // 조명 설정
    {
        // 조명 0은 고정
        m_globalConstsCPU.lights[0].radiance = Vector3(5.0f);
        m_globalConstsCPU.lights[0].position = Vector3(0.0f, 1.5f, 1.5f);
        m_globalConstsCPU.lights[0].direction = Vector3(0.0f, -1.0f, 0.0f);
        m_globalConstsCPU.lights[0].spotPower = 6.0f;
        m_globalConstsCPU.lights[0].type = LIGHT_SPOT | LIGHT_SHADOW; // Point with shadow

        // 조명 1의 위치와 방향은 Update()에서 설정
        m_globalConstsCPU.lights[1].radiance = Vector3(5.0f);
        m_globalConstsCPU.lights[1].spotPower = 6.0f;
        m_globalConstsCPU.lights[1].fallOffEnd = 20.0f;
        m_globalConstsCPU.lights[1].type = LIGHT_SPOT | LIGHT_SHADOW; // Point with shadow

        // 조명 2는 꺼놓음
        m_globalConstsCPU.lights[2].type = LIGHT_OFF;
    }

    // 조명 위치 표시
    {
        for (int i = 0; i < MAX_LIGHTS; i++) {
            MeshData sphere = GeometryGenerator::MakeSphere(1.0f, 20, 20);
            m_lightSphere[i] = make_shared<Model>(m_device, m_context, vector{sphere});
            m_lightSphere[i]->UpdateWorldRow(
                Matrix::CreateTranslation(m_globalConstsCPU.lights[i].position));
            m_lightSphere[i]->m_materialConstsCPU.albedoFactor = Vector3(0.0f);
            m_lightSphere[i]->m_materialConstsCPU.emissionFactor = Vector3(1.0f, 1.0f, 0.0f);
            m_lightSphere[i]->m_castShadow = false; // 조명 표시 물체들은 그림자 X

            if (m_globalConstsCPU.lights[i].type == 0)
                m_lightSphere[i]->m_isVisible = false;

            m_basicList.push_back(m_lightSphere[i]); // 리스트에 등록
        }
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
    UpdateLights(dt);

    EngineBase::UpdateGlobalConstants(eyeWorld, viewRow, projRow, reflectionRow);

    // 거울은 따로 처리
    m_mirror->UpdateConstantBuffers(m_device, m_context);

    // 조명의 위치 반영
    for (int i = 0; i < MAX_LIGHTS; i++)
        m_lightSphere[i]->UpdateWorldRow(
            Matrix::CreateScale((std::max)(0.01f, m_globalConstsCPU.lights[i].radius)) *
            Matrix::CreateTranslation(m_globalConstsCPU.lights[i].position));

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

    if (m_rightButton) {
        // 드래그 중: 사용자가 직접 위치를 조작 (물리는 일시정지, 속도 초기화)
        translation += dragTranslation;
        m_mainPhysicsBody.position = translation;
        m_mainPhysicsBody.velocity = Vector3(0.0f);
    } else {
        // 드래그를 놓으면 중력 + 바닥 충돌 시뮬레이션 재개
        m_mainPhysicsBody.Update(dt, m_floorHeight);
    }

    // 물리 구들: 중력 + 바닥 충돌
    for (auto &body : m_physicsBodies) {
        body.Update(dt, m_floorHeight);
    }

    // 구-구 충돌 응답 (모든 쌍 + mainSphere)
    for (int i = 0; i < NUM_PHYSICS_SPHERES; i++) {
        for (int j = i + 1; j < NUM_PHYSICS_SPHERES; j++) {
            PhysicsBody::ResolveCollision(m_physicsBodies[i], m_physicsBodies[j]);
        }
        PhysicsBody::ResolveCollision(m_physicsBodies[i], m_mainPhysicsBody);
    }

    for (int i = 0; i < NUM_PHYSICS_SPHERES; i++) {
        m_physicsSphereModels[i]->UpdateWorldRow(
            Matrix::CreateFromQuaternion(m_physicsBodies[i].orientation) *
            Matrix::CreateTranslation(m_physicsBodies[i].position));
    }

    translation = m_mainPhysicsBody.position; // 충돌 응답 이후 위치로 갱신

    m_mainSphere->m_worldRow.Translation(Vector3(0.0f));
    m_mainSphere->UpdateWorldRow(m_mainSphere->m_worldRow * Matrix::CreateFromQuaternion(q) *
                                 Matrix::CreateTranslation(translation));
    m_mainBoundingSphere.Center = m_mainSphere->m_worldRow.Translation();

    // 모든 오브젝트 constant buffer 갱신 (조명은 이미 GlobalConstants에 있음)
    for (auto &i : m_basicList) {
        i->UpdateConstantBuffers(m_device, m_context);
    }
}
void UnoEngine::Render() {
    UnoEngine::SetMainViewport();

    // 공용 샘플러 바인딩 (Common.hlsli의 s0, s1)
    m_context->VSSetSamplers(0, UINT(Graphics::sampleStates.size()), Graphics::sampleStates.data());
    m_context->PSSetSamplers(0, UINT(Graphics::sampleStates.size()), Graphics::sampleStates.data());

    // 공용 IBL 텍스처 바인딩 (Common.hlsli의 t10~13)
    vector<ID3D11ShaderResourceView *> commonSRVs = {m_envSRV.Get(), m_specularSRV.Get(),
                                                     m_irradianceSRV.Get(), m_brdfSRV.Get()};
    m_context->PSSetShaderResources(10, UINT(commonSRVs.size()), commonSRVs.data());

    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    vector<ID3D11RenderTargetView *> rtvs = {m_floatRTV.Get()};
     // Depth Only Pass (RTS 생략 가능)
    m_context->OMSetRenderTargets(0, NULL, m_depthOnlyDSV.Get());
    m_context->ClearDepthStencilView(m_depthOnlyDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    EngineBase::SetPipelineState(Graphics::depthOnlyPSO);
    EngineBase::SetGlobalConsts(m_globalConstsGPU);
    for (auto &i : m_basicList)
        i->Render(m_context);
    m_skybox->Render(m_context);
    m_mirror->Render(m_context);

    // 그림자맵 만들기
    EngineBase::SetShadowViewport(); // 그림자맵 해상도
    EngineBase::SetPipelineState(Graphics::depthOnlyPSO);
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (m_globalConstsCPU.lights[i].type & LIGHT_SHADOW) {
            // RTS 생략 가능
            m_context->OMSetRenderTargets(0, NULL, m_shadowDSVs[i].Get());
            m_context->ClearDepthStencilView(m_shadowDSVs[i].Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
            EngineBase::SetGlobalConsts(m_shadowGlobalConstsGPU[i]);
            for (auto &i : m_basicList)
                if (i->m_castShadow && i->m_isVisible)
                    i->Render(m_context);
            m_skybox->Render(m_context);
            m_mirror->Render(m_context);
        }
    }

    // 다시 렌더링 해상도로 되돌리기
    EngineBase::SetMainViewport();

     // 거울 1. 거울은 빼고 원래 대로 그리기
    for (size_t i = 0; i < rtvs.size(); i++) {
        m_context->ClearRenderTargetView(rtvs[i], clearColor);
    }
    m_context->OMSetRenderTargets(UINT(rtvs.size()), rtvs.data(), m_depthStencilView.Get());

    // 그림자맵들도 공용 텍스춰들 이후에 추가
    // 주의: 마지막 shadowDSV를 RenderTarget에서 해제한 후 설정
    vector<ID3D11ShaderResourceView *> shadowSRVs;
    for (int i = 0; i < MAX_LIGHTS; i++) {
        shadowSRVs.push_back(m_shadowSRVs[i].Get());
    }
    m_context->PSSetShaderResources(15, UINT(shadowSRVs.size()), shadowSRVs.data());

    m_context->ClearDepthStencilView(m_depthStencilView.Get(),
                                     D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    EngineBase::SetPipelineState(m_isDrawAsWire ? Graphics::defaultWirePSO : Graphics::defaultSolidPSO);
    EngineBase::SetGlobalConsts(m_globalConstsGPU);

    for (auto &i : m_basicList) {
        i->Render(m_context);
    }

    EngineBase::SetPipelineState(Graphics::normalsPSO);
    for (auto &i : m_basicList) {
        if (i->m_drawNormals)
            i->RenderNormals(m_context);
    }

    EngineBase::SetPipelineState(m_isDrawAsWire ? Graphics::skyboxWirePSO : Graphics::skyboxSolidPSO);

    m_skybox->Render(m_context);

    // 거울 2. 거울 위치만 StencilBuffer에 1로 표기
    EngineBase::SetPipelineState(Graphics::stencilMaskPSO);

    m_mirror->Render(m_context);

    // 거울 3. 거울 위치에 반사된 물체들을 렌더링
    EngineBase::SetPipelineState(m_isDrawAsWire ? Graphics::reflectWirePSO : Graphics::reflectSolidPSO);
    EngineBase::SetGlobalConsts(m_reflectGlobalConstsGPU);

    m_context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    for (auto &i : m_basicList) {
        i->Render(m_context);
    }

    EngineBase::SetPipelineState(m_isDrawAsWire ? Graphics::reflectSkyboxWirePSO
                                           : Graphics::reflectSkyboxSolidPSO);
    m_skybox->Render(m_context);

    // 거울 4. 거울 자체의 재질을 "Blend"로 그림
    EngineBase::SetPipelineState(m_isDrawAsWire ? Graphics::mirrorBlendWirePSO
                                           : Graphics::mirrorBlendSolidPSO);
    EngineBase::SetGlobalConsts(m_globalConstsGPU);

    m_mirror->Render(m_context);

    m_context->ResolveSubresource(m_resolvedBuffer.Get(), 0, m_floatBuffer.Get(), 0,
                                  DXGI_FORMAT_R16G16B16A16_FLOAT);

    // PostEffects
    //EngineBase::SetPipelineState(Graphics::postEffectsPSO);

    // vector<ID3D11ShaderResourceView *> postEffectsSRVs =
    // {m_resolvedSRV.Get(),
    //                                                       m_depthOnlySRV.Get()};
    // EngineBase::SetGlobalConsts(m_globalConstsGPU);

    // 그림자맵 확인용 임시
    EngineBase::SetGlobalConsts(m_shadowGlobalConstsGPU[1]);
    //vector<ID3D11ShaderResourceView *> postEffectsSRVs = {m_resolvedSRV.Get(),
    //                                                      m_shadowSRVs[1].Get()};

    //// 20번에 넣어줌
    //m_context->PSSetShaderResources(20, UINT(postEffectsSRVs.size()), postEffectsSRVs.data());
    //m_context->OMSetRenderTargets(1, m_postEffectsRTV.GetAddressOf(), NULL);
    //m_context->PSSetConstantBuffers(3, 1, m_postEffectsConstsGPU.GetAddressOf());
    //m_screenSquare->Render(m_context);

    // 단순 이미지 처리와 블룸
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
    if (ImGui::TreeNode("Light")) {
        ImGui::SliderFloat3("Position", &m_globalConstsCPU.lights[0].position.x, -5.0f, 5.0f);
        ImGui::SliderFloat("Radius", &m_globalConstsCPU.lights[0].radius, 0.0f, 0.5f);
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
void UnoEngine::UpdateLights(float dt) {

    // 회전하는 lights[1] 업데이트
    static Vector3 lightDev = Vector3(0.8f, 0.0f, 0.0f);
    if (m_lightRotate) {
        lightDev = Vector3::Transform(lightDev, Matrix::CreateRotationY(dt * 3.141592f * 0.5f));
    }
    m_globalConstsCPU.lights[1].position = Vector3(0.0f, 0.5f, 2.0f) + lightDev;
    Vector3 focusPosition = Vector3(0.0f, -0.5f, 1.7f);
    m_globalConstsCPU.lights[1].direction = focusPosition - m_globalConstsCPU.lights[1].position;
    m_globalConstsCPU.lights[1].direction.Normalize();

    // 그림자맵을 만들기 위한 시점
    for (int i = 0; i < MAX_LIGHTS; i++) {
        const auto &light = m_globalConstsCPU.lights[i];
        if (light.type & LIGHT_SHADOW) {

            Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
            if (abs(up.Dot(light.direction) + 1.0f) < 1e-5)
                up = Vector3(1.0f, 0.0f, 0.0f);

            // 그림자맵을 만들 때 필요
            Matrix lightViewRow =
                XMMatrixLookAtLH(light.position, light.position + light.direction, up);

            Matrix lightProjRow =
                XMMatrixPerspectiveFovLH(XMConvertToRadians(120.0f), 1.0f, 0.01f, 100.0f);

            m_shadowGlobalConstsCPU[i].eyeWorld = light.position;
            m_shadowGlobalConstsCPU[i].view = lightViewRow.Transpose();
            m_shadowGlobalConstsCPU[i].proj = lightProjRow.Transpose();
            m_shadowGlobalConstsCPU[i].invProj = lightProjRow.Invert().Transpose();
            m_shadowGlobalConstsCPU[i].viewProj = (lightViewRow * lightProjRow).Transpose();

            D3D11Utils::UpdateBuffer(m_device, m_context, m_shadowGlobalConstsCPU[i],
                                     m_shadowGlobalConstsGPU[i]);

            // 그림자를 실제로 렌더링할 때 필요
            m_globalConstsCPU.lights[i].viewProj = m_shadowGlobalConstsCPU[i].viewProj;
            m_globalConstsCPU.lights[i].invProj = m_shadowGlobalConstsCPU[i].invProj;

            // 반사된 장면에서도 그림자를 그리고 싶다면 조명도 반사시켜서
            // 넣어주면 됩니다.
        }
    }
}
} // namespace hlab