#pragma once

#include "render/D3DUtil.h"
#include "sim/Particle.h"

// Owns everything the GPU needs to draw particles.
// M1: particles live in an UPLOAD-heap buffer the CPU rewrites every frame, read in shaders as
// StructuredBuffer<Particle> through an SRV. M5 swaps that buffer for one shared with CUDA.
class ParticleRenderer
{
public:
    ParticleRenderer() = default;
    ~ParticleRenderer();
    ParticleRenderer(const ParticleRenderer&) = delete;
    ParticleRenderer& operator=(const ParticleRenderer&) = delete;

    void Init(ID3D12Device* device, UINT maxParticles);

    // Copies particles into the mapped buffer. Only safe while the GPU isn't reading it:
    // true today because SandForge::WaitForPreviousFrame drains the GPU every frame.
    void Upload(const Particle* particles, UINT count);

    UINT ParticleCount() const { return m_particleCount; }
    ID3D12DescriptorHeap* SrvHeap() const { return m_srvHeap.Get(); }
    // Start of the descriptor table: slot 0 is the particle SRV (t0)
    D3D12_GPU_DESCRIPTOR_HANDLE ParticleSrv() const { return m_srvHeap->GetGPUDescriptorHandleForHeapStart(); }

    // M1 tasks 3/4: root signature, PSO and a Record(commandList) that draws go here.

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_particleBuffer; // UPLOAD heap, persistently mapped
    Particle* m_mappedParticles = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;  // shader-visible CBV/SRV/UAV heap
    UINT m_maxParticles = 0;
    UINT m_particleCount = 0;
};
