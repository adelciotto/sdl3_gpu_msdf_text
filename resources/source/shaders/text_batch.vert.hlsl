struct Instance_Data {
  float3 position;
  float  padding_0;
  float4 rotation;
  float2 scale;
  float2 padding_1;
  float4 atlas_quad;
  float4 color;
};

StructuredBuffer<Instance_Data> Data_Buffer : register(t0, space0);

struct Output {
  float2 texcoord : TEXCOORD0;
  float4 color : TEXCOORD1;
  float4 position : SV_Position;
};

cbuffer Uniform_Block : register(b0, space1) {
  float4x4 world_to_clip_transform : packoffset(c0);
  uint first_instance : packoffset(c4);
}

static const uint TRIANGLE_INDICES[6] = {0, 1, 2, 3, 2, 1};
static const float3 VERTICES[4] = {
  {0.0f, 0.0f, 0.0f},
  {1.0f, 0.0f, 0.0f},
  {0.0f, 1.0f, 0.0f},
  {1.0f, 1.0f, 0.0f}
};

Output main(uint id: SV_VertexID) {
  uint instance_index = first_instance + (id / 6);
  uint vertex_index = TRIANGLE_INDICES[instance_index % 6];
  Instance_Data instance = Data_Buffer[instance_index];

  float2 texcoord[4] = {
    {instance.atlas_quad.x, instance.atlas_quad.y},
    {instance.atlas_quad.x + instance.atlas_quad.z, instance.atlas_quad.y},
    {instance.atlas_quad.x, instance.atlas_quad.y + instance.atlas_quad.w},
    {instance.atlas_quad.x + instance.atlas_quad.z, instance.atlas_quad.y + instance.atlas_quad.w}
  };

  float3 coord = VERTICES[vertex_index];
  coord *= float3(instance.scale, 1.0f);
  coord += instance.position;

  Output output;
  output.position = mul(world_to_clip_transform, float4(coord, 1.0f));
  output.texcoord = texcoord[vertex_index];
  output.color = instance.color;

  return output;
}
