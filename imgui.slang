struct VertInput {
  float2 pos   : Position;
  float2 uv    : TexCoord0;
  float4 color : Color0;
};

struct V2F {
  float4 pos   : SV_Position;
  float4 color : Color0;
  float2 uv    : TexCoord0;
};

struct FontParams {
  Texture2D atlas;
  SamplerState sampler;
};

struct CoordData {
  float2 scale;
  float2 translation;
};

ParameterBlock<FontParams> fontParams;

ParameterBlock<CoordData> coordData;

[shader("vertex")]
V2F vertMain(VertInput v)
{
  V2F out;
  out.pos = float4(v.pos * coordData.scale + coordData.translation, 0, 1);
  out.color = v.color;
  out.uv = v.uv;

  return out;
}

[shader("fragment")]
float4 fragMain(V2F v2f) : SV_Target0
{
  return v2f.color * fontParams.atlas.Sample(fontParams.sampler, v2f.uv);
}
