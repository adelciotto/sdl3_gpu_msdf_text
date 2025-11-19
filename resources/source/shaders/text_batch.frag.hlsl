struct Input {
    float2 texcoord : TEXCOORD0;
    float4 rolor : TEXCOORD1;
};

float4 main(Input input) : SV_Target0 {
  return float4(0, 0, 0, 0);
}
