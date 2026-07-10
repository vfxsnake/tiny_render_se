
struct VertexOutput {
    // sv stands for system value.
    float4 sv_position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VertexOutput vertMain(uint vertex_id : SV_VertexID) {
    VertexOutput output;

    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.sv_position = float4(uv * 2.0 - 1.0, 0.0, 1.0);
    output.uv = uv;
    return output;
}

// sampler
[[vk::binding(0, 0)]] 
Sampler2D texture;

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    return texture.Sample(inVert.uv);
}