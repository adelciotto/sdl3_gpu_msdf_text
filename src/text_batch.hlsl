#ifdef VERTEX_SHADER
struct Glpyh_Data {
  float3 position;
  float  size;
  float4 plane_bounds;
  float4 atlas_bounds;
  float4 color;
  float4 outline_color;
  float  outline_thickness;
};

StructuredBuffer<Glpyh_Data> Data_Buffer : register(t0, space0);

struct Output {
  nointerpolation float4 color : TEXCOORD0;
  nointerpolation float4 outline_color : TEXCOORD1;
  nointerpolation float4 params : TEXCOORD2;
  float2                 texcoord : TEXCOORD3;
  float4                 position : SV_Position;
};

cbuffer Uniform_Block : register(b0, space1) {
  float4x4 world_to_clip_transform : packoffset(c0);
  uint     first_glyph : packoffset(c4);
}

static const uint TRIANGLE_INDICES[6] = {0, 1, 2, 3, 2, 1};

Output main(uint id : SV_VertexID) {
  uint       glyph_index  = id / 6;
  uint       vertex_index = TRIANGLE_INDICES[id % 6];
  Glpyh_Data glyph        = Data_Buffer[first_glyph + glyph_index];

  float x0 = glyph.position.x + glyph.plane_bounds.x * glyph.size;
  float y0 = glyph.position.y + glyph.plane_bounds.y * glyph.size;
  float x1 = glyph.position.x + glyph.plane_bounds.z * glyph.size;
  float y1 = glyph.position.y + glyph.plane_bounds.w * glyph.size;

  float2 vertex_position[4] = {
      {x0, y0},
      {x1, y0},
      {x0, y1},
      {x1, y1},
  };
  float2 vertex_texcoord[4] = {
      {glyph.atlas_bounds.x, glyph.atlas_bounds.y},
      {glyph.atlas_bounds.z, glyph.atlas_bounds.y},
      {glyph.atlas_bounds.x, glyph.atlas_bounds.w},
      {glyph.atlas_bounds.z, glyph.atlas_bounds.w}};

  Output output;
  output.position =
      mul(world_to_clip_transform, float4(vertex_position[vertex_index], glyph.position.z, 1.0f));
  output.texcoord      = vertex_texcoord[vertex_index];
  output.color         = glyph.color;
  output.outline_color = glyph.outline_color;
  output.params        = float4(glyph.outline_thickness, 0.0f, 0.0f, 0.0f);

  return output;
}
#endif

#ifdef FRAGMENT_SHADER
Texture2D<float4> Texture : register(t0, space2);
SamplerState      Sampler : register(s0, space2);

struct Input {
  nointerpolation float4 color : TEXCOORD0;
  nointerpolation float4 outline_color : TEXCOORD1;
  nointerpolation float4 params : TEXCOORD2;
  float2                 texcoord : TEXCOORD3;
};

cbuffer Uniform_Block : register(b0, space3) {
  float  font_size : packoffset(c0);
  float2 unit_range : packoffset(c0.y);
}

float screen_pixel_range(float2 texcoord) {
  float2 dx              = ddx(texcoord);
  float2 dy              = ddy(texcoord);
  float2 screen_tex_size = 1.0f / sqrt(dx * dx + dy * dy);
  return max(0.5f * dot(unit_range, screen_tex_size), 1.0f);
}

float median(float r, float g, float b) {
  return max(min(r, g), min(max(r, g), b));
}

float4 main(Input input) : SV_Target0 {
  float3 msd = Texture.Sample(Sampler, input.texcoord).rgb;
  float  sd  = median(msd.r, msd.g, msd.b);

  float outline_thickness = input.params.x;
  if (outline_thickness > 0.0f) {
    if (sd <= 0.0001f) { discard; }

    float px_range = screen_pixel_range(input.texcoord);

    static const float mid_body_thickness = -0.1f;
    sd += -0.5f + mid_body_thickness;

    float body_px_dist = px_range * sd;
    float body_opacity = smoothstep(-0.5f, 0.5f, body_px_dist);

    float char_px_dist = px_range * (sd + outline_thickness);
    float char_opacity = smoothstep(-0.5f, 0.5f, char_px_dist);

    float outline_opacity = char_opacity - body_opacity;

    float3 color = lerp(input.outline_color.rgb, input.color.rgb, body_opacity);
    float  alpha = body_opacity * input.color.a + outline_opacity * input.outline_color.a;

    color *= alpha;

    return float4(color, alpha);
  }

  float screen_px_dist = screen_pixel_range(input.texcoord) * (sd - 0.5f);
  float opacity        = clamp(screen_px_dist + 0.5f, 0.0f, 1.0f);

  float4 color = input.color;
  color.a *= opacity;
  color.rgb *= color.a;

  return color;
}
#endif
