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

struct TexParams {
  Texture2D atlas;
  SamplerState sampler;
};

ParameterBlock<TexParams> texParams;

[shader("vertex")]
V2F vertMain(VertInput v)
{
  V2F out;
  out.pos = float4(v.pos, 0, 1);
  //gl_Position = vec4(aPos * pc.uScale + pc.uTranslate, 0, 1);
  out.color = v.color;
  out.uv = v.uv;

  return out;
}

[shader("fragment")]
float4 fragMain(V2F v2f) : SV_Target0
{
  return v2f.color * texParams.atlas.Sample(texParams.sampler, v2f.uv);
}
