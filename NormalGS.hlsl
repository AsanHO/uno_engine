cbuffer BasicVertexConstantData : register(b0)
{
    matrix modelWorld;
    matrix invTranspose;
    matrix view;
    matrix projection;
};

cbuffer NormalVertexConstantData : register(b1)
{
    float scale; // 그려지는 선분의 길이 조절
};

struct GeometryShaderInput
{
    float4 posModel : SV_POSITION;
    float3 normalModel : NORMAL;
};

struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float3 color : COLOR;
};

[maxvertexcount(2)]
void main(point GeometryShaderInput input[1], inout LineStream<PixelShaderInput> outputStream)
{
    PixelShaderInput output;
    
    float4 posWorld = mul(input[0].posModel, modelWorld);
    float4 normalModel = float4(input[0].normalModel, 0.0);
    float4 normalWorld = mul(normalModel, invTranspose);
    normalWorld = float4(normalize(normalWorld.xyz), 0.0);
    
    output.pos = mul(posWorld, view);
    output.pos = mul(output.pos, projection);
    output.color = float3(1.0, 1.0, 0.0);
    outputStream.Append(output);
    
    output.pos = mul(posWorld + scale * normalWorld, view);
    output.pos = mul(output.pos, projection);
    output.color = float3(1.0, 0.0, 0.0);
    outputStream.Append(output);
}
