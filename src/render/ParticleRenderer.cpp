#include "stdafx.h"
#include "render/ParticleRenderer.h"

ParticleRenderer::~ParticleRenderer()
{
    // The owner must have drained the GPU before this runs (SandForge::OnDestroy does)
    if (m_particleBuffer != nullptr && m_mappedParticles != nullptr)
    {
        m_particleBuffer->Unmap(0, nullptr);
        m_mappedParticles = nullptr;
    }
}

void ParticleRenderer::Init(ID3D12Device* device, UINT maxParticles)
{
    m_maxParticles = maxParticles;

    // 1. The memory. An UPLOAD buffer starts (and stays) in GENERIC_READ, which already includes
    //    the shader-resource states, so no barrier is ever needed before a shader reads it.
    m_particleBuffer = D3DUtil::CreateBuffer(
        device,
        D3D12_HEAP_TYPE_UPLOAD,
        UINT64(maxParticles) * sizeof(Particle),
        D3D12_RESOURCE_STATE_GENERIC_READ);

    // Map once and keep the pointer: mapping UPLOAD memory is free to hold for the resource's lifetime
    D3D12_RANGE readRange = { 0, 0 }; // the CPU never reads it back
    ThrowIfFailed(m_particleBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedParticles)));

    // 2. Somewhere to put the view. Shaders can only see descriptors in a SHADER_VISIBLE heap,
    //    and only one CBV/SRV/UAV heap can be bound at a time, so later views (UAVs, more SRVs)
    //    should be added to this heap rather than a new one.
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap)));

    // 3. The view itself: tells the GPU how to interpret raw buffer bytes as StructuredBuffer<Particle>.
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;                    // structured buffers have no format...
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = maxParticles;
    srvDesc.Buffer.StructureByteStride = sizeof(Particle);  // ...the stride defines the element instead
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    // Descriptors are written through the CPU handle; shaders find them through the GPU handle (ParticleSrv)
    device->CreateShaderResourceView(m_particleBuffer.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
}

void ParticleRenderer::Upload(const Particle* particles, UINT count)
{
    m_particleCount = (std::min)(count, m_maxParticles);
    memcpy(m_mappedParticles, particles, size_t(m_particleCount) * sizeof(Particle));
}
