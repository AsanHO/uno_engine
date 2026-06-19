TextureCube g_envTex : register(t0);
TextureCube g_specularTex : register(t1);
TextureCube g_irradianceTex : register(t2);
SamplerState g_sampler : register(s0);

cbuffer PixelConstData : register(b0)
{
    int textureToDraw = 0; // 0: Env, 1: Specular, 2: Irradiance
    float mipLevel = 0.0f;
    float dummy1;
    float dummy2;
};

struct CubeMappingPixelShaderInput
{
    float4 posProj : SV_POSITION;
    float3 posModel : POSITION;
};

struct PixelShaderOutput
{
    float4 pixelColor : SV_Target0;
    float4 indexColor : SV_Target1;
};

PixelShaderOutput main(CubeMappingPixelShaderInput input)
{
    PixelShaderOutput output;
    
    if (textureToDraw == 0)
    {
        output.pixelColor = g_envTex.SampleLevel(g_sampler, input.posModel.xyz, mipLevel);
    }
    else if (textureToDraw == 1)
    {
        output.pixelColor = g_specularTex.SampleLevel(g_sampler, input.posModel.xyz, mipLevel);
    }
    else
    {
        output.pixelColor = g_irradianceTex.SampleLevel(g_sampler, input.posModel.xyz, mipLevel);
    }
    
    output.indexColor = float4(0.0, 0.0, 1.0, 0.0);
    
    return output;
}