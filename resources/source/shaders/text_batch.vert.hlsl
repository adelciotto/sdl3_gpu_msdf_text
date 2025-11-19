struct Output {
  float2 texcoord : TEXCOORD0;
  float4 color : TEXCOORD1;
  float4 position : SV_Position;
};

Output main(uint id: SV_VertexID) {
  Output output;

  return output;
}
