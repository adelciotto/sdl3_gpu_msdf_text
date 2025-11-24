#ifdef VERTEX_SHADER
struct Instance_Data {
  float3 position;
  float size;
  float4 rotation;
  float4 fg_color;
  float4 bg_color;
  float4 plane_bounds;
  float4 atlas_bounds;
};

StructuredBuffer<Instance_Data> Data_Buffer : register(t0, space0);

struct Output {
  float2 texcoord : TEXCOORD0;
  float4 fg_color : TEXCOORD1;
  float4 bg_color : TEXCOORD2;
  float size : TEXCOORD3;
  float4 position : SV_Position;
};

cbuffer Uniform_Block : register(b0, space1) {
  float4x4 world_to_clip_transform : packoffset(c0);
  uint first_instance : packoffset(c4);
}

static const uint TRIANGLE_INDICES[6] = {0, 1, 2, 3, 2, 1};

Output main(uint id: SV_VertexID) {
  uint instance_index = id / 6;
  uint vertex_index = TRIANGLE_INDICES[id % 6];
  Instance_Data instance = Data_Buffer[first_instance + instance_index];

  float x0 = instance.position.x + instance.plane_bounds.x * instance.size;
  float y0 = instance.position.y - instance.plane_bounds.y * instance.size;
  float x1 = instance.position.x + instance.plane_bounds.z * instance.size;
  float y1 = instance.position.y - instance.plane_bounds.w * instance.size;

  float2 vertex_position[4] = {
    {x0, y0},
    {x1, y0},
    {x0, y1},
    {x1, y1},
  };
  float2 vertex_texcoord[4] = {
    {instance.atlas_bounds.x, instance.atlas_bounds.y},
    {instance.atlas_bounds.z, instance.atlas_bounds.y},
    {instance.atlas_bounds.x, instance.atlas_bounds.w},
    {instance.atlas_bounds.z, instance.atlas_bounds.w}
  };

  Output output;
  output.position = mul(world_to_clip_transform, float4(vertex_position[vertex_index], instance.position.z, 1.0f));
  output.texcoord = vertex_texcoord[vertex_index];
  output.fg_color = instance.fg_color;
  output.bg_color = instance.bg_color;
  output.size = instance.size;

  return output;
}
#endif

#ifdef FRAGMENT_SHADER
Texture2D<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

struct Input {
  float2 texcoord : TEXCOORD0;
  float4 fg_color : TEXCOORD1;
  float4 bg_color : TEXCOORD2;
  float size : TEXCOORD3;
};

cbuffer Uniform_Block : register(b0, space3) {
  float font_size : packoffset(c0);
  float pixel_range : packoffset(c0.y);
}

float screen_pixel_range(float size) {
  return size / font_size * 2.0f;
}

float median(float r, float g, float b) {
  return max(min(r, g), min(max(r, g), b));
}

float4 main(Input input) : SV_Target0 {
  float3 msd = Texture.Sample(Sampler, input.texcoord).rgb;
  float sd = median(msd.r, msd.g, msd.b);
  float screen_px_distance = screen_pixel_range(input.size) * (sd - 0.5);
  float opacity = clamp(screen_px_distance + 0.5, 0.0, 1.0);
  return lerp(input.bg_color, input.fg_color, opacity);
}
#endif
