// A rectangle of the target, as two triangles made from the vertex number (no vertex buffer): draw 6 vertices.
//
// Bindings (SDL GPU, SPIR-V): vertex uniforms in space1.

cbuffer Quad : register(b0, space1)
{
    // The rectangle in target pixels: left, top, width, height.
    float4 rect;
    // The target's size in pixels (xy).
    float4 targetSize;
};

static const float2 corners[6] = {
    float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0),
    float2(0.0, 1.0), float2(1.0, 0.0), float2(1.0, 1.0),
};

float4 main(uint vertex : SV_VertexID) : SV_Position
{
    const float2 pixel = rect.xy + corners[vertex] * rect.zw;
    float2 ndc = pixel / targetSize.xy * 2.0 - 1.0;
    // Target rows go down; NDC's y goes up.
    ndc.y = -ndc.y;
    return float4(ndc, 0.0, 1.0);
}
