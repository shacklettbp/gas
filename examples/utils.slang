public float4 worldToClip(float3x4 w2c, float3 pos_w, float z_near)
{
  return float4(
    dot(pos_w, w2c[0].xyz) + w2c[0].w,
    dot(pos_w, w2c[1].xyz) + w2c[1].w,
    z_near,
    dot(pos_w, w2c[2].xyz) + w2c[2].w);
}

public float3 objectPositionToWorld(float3x4 o2w, float3 pos_o)
{
  return float3(
    dot(pos_o, o2w[0].xyz) + o2w[0].w,
    dot(pos_o, o2w[1].xyz) + o2w[1].w,
    dot(pos_o, o2w[2].xyz) + o2w[2].w);
}

public float3 objectNormalToWorld(float3x4 w2o, float3 normal_o)
{
  return w2o[0].xyz * normal_o.x +
         w2o[1].xyz * normal_o.y +
         w2o[2].xyz * normal_o.z;
}

public void cubeVert(uint id, out float3 pos_out, out float3 normal_out)
{
  uint tri = id / 3;
  uint idx = id % 3;
  uint face = tri / 2;
  uint top = tri % 2;

  uint dir = face % 3;
  uint pos = face / 3;

  uint nz = dir >> 1;
  uint ny = dir & 1;
  uint nx = 1 ^ (ny | nz);

  float3 d = {nx, ny, nz};
  float flip = 1.0 - 2.0 * pos;

  float3 n = flip * d;
  float3 u = -d.yzx;
  float3 v = flip * d.zxy;

  float mirror = -1.0 + 2.0 * top;
  pos_out = n +
    mirror * (1.0 - 2 * (idx & 1)) * u +
    mirror * (1.0 - 2 * (idx >> 1)) * v;

  normal_out = n;
}

public interface IVertexFetch {
  float3 fetch(uint idx);
};

public interface IObject2ClipTransform {
  float4 object2Clip(float3 pos_o);
};

// Taken from github.com/m-schuetz/webgpu_wireframe_thicklines
public float4 wireframeVertexShaderTransform<VertexFetchT, TransformT>(
    uint id,
    float line_width,
    uint2 fb_dims,
    VertexFetchT vertex_fetcher,
    TransformT txfm)
  where VertexFetchT : IVertexFetch
  where TransformT : IObject2ClipTransform
{
  uint tri_idx = id / 18;
  uint local_vert_idx = id % 18;
  uint local_line_idx = local_vert_idx / 6;

  uint start_elem_idx = 3 * tri_idx + local_line_idx;
  uint end_elem_idx = 3 * tri_idx + (local_line_idx + 1) % 3;

  float3 start = vertex_fetcher.fetch(start_elem_idx);
  float3 end = vertex_fetcher.fetch(end_elem_idx);

  uint local_idx = id % 6;

  float4 clip_pos;
  float2 screen_pos;
  float2 screen_dir;
  {
    float4 start_clip_pos = txfm.object2Clip(start);
    float4 end_clip_pos = txfm.object2Clip(end);

    float2 start_screen_pos = start_clip_pos.xy / start_clip_pos.w;
    float2 end_screen_pos = end_clip_pos.xy / end_clip_pos.w;

    if (local_idx == 0 || local_idx == 3 || local_idx == 5) {
      clip_pos = start_clip_pos;
      screen_pos = start_screen_pos;
    } else {
      clip_pos = end_clip_pos;
      screen_pos = end_screen_pos;
    }

    screen_dir = normalize(end_screen_pos - start_screen_pos);
  }

  {
    float2 px_offset;

    if (local_idx == 0 || local_idx == 1 || local_idx == 3) {
      px_offset = { screen_dir.y, -screen_dir.x };
    } else {
      px_offset = { -screen_dir.y, screen_dir.x };
    }

    if (local_idx == 0 || local_idx == 3 || local_idx == 5) {
      px_offset -= screen_dir;
    } else {
      px_offset += screen_dir;
    }

    float2 adjusted = screen_pos + line_width * px_offset / float2(fb_dims);
    clip_pos = float4(adjusted * clip_pos.w, clip_pos.zw);
  }

  return clip_pos;
}
