struct TriColor {
  float3 color;
};

ParameterBlock<TriColor> perDraw;

[shader("vertex")]
float4 vertMain(uint i : SV_VertexID) : SV_Position
{
  float2 uv = float2(i == 2 ? 2 : 0, i == 1 ? 2 : 0);
	return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}

[shader("fragment")]
float4 fragMain() : SV_Target0
{
  float3 base_color = perDraw.color.rgb;

  return float4(base_color, 1);
}
