#pragma once
#include <DirectXMath.h>

// One particle as the GPU sees it. The layout must match every reader:
//   HLSL: struct Particle { float3 position; float shade; };  StructuredBuffer<Particle>
//   CUDA (M4/M5): a float4 per particle
struct Particle
{
    DirectX::XMFLOAT3 position; // simulation space, domain is [0,1]^3
    float shade;                // brightness multiplier, ~[0.75, 1.25]
};
static_assert(sizeof(Particle) == 16, "Particle must stay 16 bytes: it is a StructuredBuffer stride and a CUDA float4");
