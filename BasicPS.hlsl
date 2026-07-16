#include "Common.hlsli" // 쉐이더에서도 include 사용 가능

// 참고자료
// https://github.com/Nadrin/PBR/blob/master/data/shaders/hlsl/pbr.hlsl

TextureCube specularIBLTex : register(t0); // 환경의 반사광 (roughness별 블러된 큐브맵)
TextureCube irradianceIBLTex : register(t1); // 환경의 확산광 (디퓨즈용 적분된 큐브맵)
Texture2D brdfTex : register(t2); // 환경 BRDF 룩업테이블
Texture2D albedoTex : register(t3); // 기본 색상 텍스처
Texture2D normalTex : register(t4); // 노멀맵
Texture2D aoTex : register(t5); // Ambient Occlusion 맵
Texture2D metallicTex : register(t6); // 금속성 맵
Texture2D roughnessTex : register(t7); // 거칠기 맵

SamplerState linearSampler : register(s0); // 일반 텍스처용, Wrap 모드
SamplerState clampSampler : register(s1); // brdf LUT용, Clamp 모드

static const float3 Fdielectric = 0.04; // 비금속(Dielectric) 재질의 F0

cbuffer BasicPixelConstData : register(b0)
{
    Material material;
    Light light[MAX_LIGHTS];
    int useAlbedoMap;
    int useNormalMap;
    int useAOMap; // Ambient Occlusion
    int invertNormalMapY;
    int useMetallicMap;
    int useRoughnessMap;
    int useEmissiveMap;
    float exposure;
    float gamma;
    float mipmapLevel;
    float2 dummy0;
};

cbuffer BasicPixelConstData : register(b1)
{
    matrix viewProj;
    float3 eyeWorld;
    float dummy1;
};

float3 SchlickFresnel(float3 F0, float NdotH)
{
    // TODO: 방정식 (5)
    return F0 + (1.0 - F0) * pow(2.0, (-5.55473 * (NdotH) - 6.98316) * NdotH);

}

struct PixelShaderOutput
{
    float4 pixelColor : SV_Target0;
};

float3 GetNormal(PixelShaderInput input)
{
    float3 normalWorld = input.normalWorld;
    
    if (useNormalMap) // NormalWorld를 교체
    {
        float3 normal = normalTex.SampleLevel(linearSampler, input.texcoord, 0.0).rgb;
        normal = 2.0 * normal - 1.0; // 범위 조절 [-1.0, 1.0]

        // OpenGL 용 노멀맵일 경우에는 y 방향을 뒤집어줍니다.
        normal.y = invertNormalMapY ? -normal.y : normal.y;
        
        float3 N = normalWorld;
        float3 T = normalize(input.tangentWorld - dot(input.tangentWorld, N) * N);
        float3 B = cross(N, T);
        
        // matrix는 float4x4, 여기서는 벡터 변환용이라서 3x3 사용
        float3x3 TBN = float3x3(T, B, N);
        normalWorld = normalize(mul(normal, TBN));
    }
    
    return normalWorld;
}

float3 DiffuseIBL(float3 albedo, float3 normalWorld, float3 pixelToEye,
                  float metallic)
{
    float3 F0 = lerp(Fdielectric, albedo, metallic);
    float3 F = SchlickFresnel(F0, max(0.0, dot(normalWorld, pixelToEye)));
    float3 kd = lerp(1.0 - F, 0.0, metallic);
    
    // 앞에서 사용했던 방법과 동일
    float3 irradiance = irradianceIBLTex.Sample(linearSampler, normalWorld).rgb;
    
    return kd * albedo;
}

float3 SpecularIBL(float3 albedo, float3 normalWorld, float3 pixelToEye,
                   float metallic, float roughness)
{
    // TODO: 슬라이드 Environment BRDF
    float2 specularBRDF = brdfTex.Sample(clampSampler, float2(dot(normalWorld, pixelToEye), roughness)).rg;
    
    // 앞에서 사용했던 방법과 동일
    float3 specularIrradiance = specularIBLTex.SampleLevel(linearSampler, reflect(-pixelToEye, normalWorld), roughness * 5.0f).rgb;
    const float3 Fdielectric = 0.04; // 비금속(Dielectric) 재질의 F0
    float3 F0 = lerp(Fdielectric, albedo, metallic);

    return (F0 * specularBRDF.r + specularBRDF.g) * specularIrradiance;

}

float3 AmbientLightingByIBL(float3 albedo, float3 normalW, float3 pixelToEye, float ao,
                            float metallic, float roughness)
{
    float3 diffuseIBL = DiffuseIBL(albedo, normalW, pixelToEye, metallic);
    float3 specularIBL = SpecularIBL(albedo, normalW, pixelToEye, metallic, roughness);
    
    return (diffuseIBL + specularIBL) * ao;
}

// GGX/Towbridge-Reitz normal distribution function.
// Uses Disney's reparametrization of alpha = roughness^2.
float NdfGGX(float NdotH, float roughness)
{
    // TODO: 방정식 (3)
    float alpha = pow(roughness, 2.0);
    float numerator = pow(alpha, 2.0);
    float denomerator = 3.141592 * pow(pow(NdotH, 2.0) * (numerator - 1) + 1, 2.0);
    return numerator / denomerator;
}

// TODO: 방정식 (4)
float SchlickG1(float NdotV, float k)
{
    return NdotV / (NdotV * (1 - k) + k);
}

float SchlickGGX(float NdotI, float NdotO, float roughness)
{
    float k = pow(roughness + 1.0, 2.0) / 8;
    return SchlickG1(NdotI, k) * SchlickG1(NdotO, k);
}

PixelShaderOutput main(PixelShaderInput input)
{
    float3 pixelToEye = normalize(eyeWorld - input.posWorld);
    float3 normalWorld = GetNormal(input);

    float3 albedo = useAlbedoMap ? albedoTex.Sample(linearSampler, input.texcoord).rgb 
                                 : material.albedo;
    float ao = useAOMap ? aoTex.SampleLevel(linearSampler, input.texcoord, 0.0).r : 1.0;
    float metallic = useMetallicMap ? metallicTex.Sample(linearSampler, input.texcoord).r 
                                    : material.metallic;
    float roughness = useRoughnessMap ? roughnessTex.Sample(linearSampler, input.texcoord).r 
                                      : material.roughness;

    float3 ambientLighting = AmbientLightingByIBL(albedo, normalWorld, pixelToEye, ao,
                                                  metallic, roughness);

    float3 directLighting = float3(0, 0, 0);

    // 포인트 라이트만 먼저 구현
    [unroll]
    for (int i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i)
    {
        float3 lightVec = light[i].position - input.posWorld;
        float3 halfway = normalize(pixelToEye + lightVec);
        
        float NdotI = max(0.0, dot(normalWorld, lightVec));
        float NdotH = max(0.0, dot(normalWorld, halfway));
        float NdotO = max(0.0, dot(normalWorld, pixelToEye));
        

        float3 F0 = lerp(Fdielectric, albedo, metallic);
        float3 F = SchlickFresnel(F0, max(0.0, dot(halfway, pixelToEye)));
        float3 kd = lerp(float3(1, 1, 1) - F, float3(0, 0, 0), metallic);
        float3 diffuseBRDF = kd * albedo;

        float D = NdfGGX(NdotH, roughness);
        float3 G = SchlickGGX(NdotI, NdotO, roughness);
        
        // 방정식 (2), 0으로 나누기 방지
        float3 specularBRDF = (F * D * G) / max(1e-5, 4.0 * NdotI * NdotO);

        float3 radiance = light[i].radiance * saturate((light[i].fallOffEnd - length(lightVec)) / (light[i].fallOffEnd - light[i].fallOffStart));

        directLighting += (diffuseBRDF + specularBRDF) * radiance * NdotI;
    }
    
    PixelShaderOutput output;
    output.pixelColor = float4(ambientLighting + directLighting, 1.0);
    output.pixelColor = clamp(output.pixelColor, 0.0, 1000.0);
    
    return output;
}
