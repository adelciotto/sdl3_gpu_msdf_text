#ifdef VERTEX_SHADER
struct Instance_Data {
  float3 position;
  float size;
  float4 color;
  float4 plane_bounds;
  float4 atlas_bounds;
};

StructuredBuffer<Instance_Data> Data_Buffer : register(t0, space0);

struct Output {
  float2 texcoord : TEXCOORD0;
  nointerpolation float4 color : TEXCOORD1;
  nointerpolation float size : TEXCOORD2;
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
  float y0 = instance.position.y + instance.plane_bounds.y * instance.size;
  float x1 = instance.position.x + instance.plane_bounds.z * instance.size;
  float y1 = instance.position.y + instance.plane_bounds.w * instance.size;

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
  output.size = instance.size;
  output.color = instance.color;

  return output;
}
#endif

#ifdef FRAGMENT_SHADER
Texture2D<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

struct Input {
  float2 texcoord : TEXCOORD0;
  nointerpolation float4 color : TEXCOORD1;
  nointerpolation float size : TEXCOORD2;
};

cbuffer Uniform_Block : register(b0, space3) {
  float font_size : packoffset(c0);
  float2 unit_range : packoffset(c0.y);
#if defined(EFFECT_OUTLINE)
  float4 outline_color : packoffset(c1.x);
  float outline_thickness : packoffset(c2.x);
#endif
}

float screen_pixel_range(float2 texcoord, float size) {
  float2 screen_tex_size = 1.0f / fwidth(texcoord);
  return max(0.5f * dot(unit_range, screen_tex_size), 1.0f);
}

float median(float r, float g, float b) {
  return max(min(r, g), min(max(r, g), b));
}

float4 main(Input input) : SV_Target0 {
#if defined(EFFECT_BASIC)
  float3 msd = Texture.Sample(Sampler, input.texcoord).rgb;
  float sd = median(msd.r, msd.g, msd.b);
  float screen_px_dist = screen_pixel_range(input.texcoord, input.size) * (sd - 0.5);
  float opacity = clamp(screen_px_dist + 0.5, 0.0, 1.0);

  float4 color = input.color;
  color.a *= opacity;
  color.rgb *= color.a;

  return color;
#elif defined(EFFECT_OUTLINE)
  float3 msd = Texture.Sample(Sampler, input.texcoord).rgb;
  float sd = median(msd.r, msd.g, msd.b);
  if (sd <= 0.0001f) {
    discard;
  }

  float px_range = screen_pixel_range(input.texcoord, input.size);

  static const float mid_body_thickness = -0.1f;
  sd += -0.5f + mid_body_thickness;

  float body_px_dist = px_range * sd;
  float body_opacity = smoothstep(-0.5f, 0.5f, body_px_dist);

  float char_px_dist = px_range * (sd + outline_thickness);
  float char_opacity = smoothstep(-0.5f, 0.5f, char_px_dist);

  float outline_opacity = char_opacity - body_opacity;

  float3 color = lerp(outline_color.rgb, input.color.rgb, body_opacity);
  float alpha = body_opacity * input.color.a + outline_opacity * outline_color.a;

  color *= alpha;

  return float4(color, alpha);
#endif
}
#endif
