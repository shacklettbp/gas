import global_param_block;

struct TriColor {
  float3 color;
};

ParameterBlock<TriColor> perDraw;

[shader("vertex")]
float4 vertMain(uint i : SV_VertexID) : SV_Position
{
  const float2 pos[] = { float2(0, 1), float2(-1, -1), float2(1, -1) };
  return float4(pos[i], 0, 1);
}

[shader("fragment")]
float4 fragMain() : SV_Target0
{
  float3 base_color = globalParams.color.rgb * perDraw.color.rgb;

  return float4(base_color, 1);
}
