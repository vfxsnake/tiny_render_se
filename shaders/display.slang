
struct VertexOutput {
    // sv stands for system value.
    float4 sv_position : SV_Position;
};

[shader("vertex")]
VertexOutput vertMain(uint vertex_id : SV_VertexID) {
    VertexOutput output;

    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);
    output.sv_position = float4(uv * 2.0 - 1.0, 0.0 , 1.0);
    return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput inVert) : SV_Target
{
    return float4(0.0, 1.0, 0.0, 1.0);
}