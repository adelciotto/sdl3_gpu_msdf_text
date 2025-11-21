Texture2D<float4> Texture : register(t0, space2);
SamplerState Sampler : register(s0, space2);

struct Input {
  float2 texcoord : TEXCOORD0;
  float4 color : TEXCOORD1;
};

cbuffer Uniform_Block : register(b0, space3) {
  float screen_px_range : packoffset(c0);
}

float median(float r, float g, float b) {
  return max(min(r, g), min(max(r, g), b));
}

float4 main(Input input) : SV_Target0 {
  float3 msd = Texture.Sample(Sampler, input.texcoord).rgb;
  float sd = median(msd.r, msd.g, msd.b);
  float screen_px_distance = screen_px_range * (sd - 0.5);
  float opacity = clamp(screen_px_distance + 0.5, 0.0, 1.0);
  return lerp(float4(0.0f, 0.0f, 0.0f, 0.0f), input.color, opacity);
}
