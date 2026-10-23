#pragma once

#include "DXSample.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

class SandForge : public DXSample {
public:
    SandForge(UINT width, UINT height, std::wstring name);

    void OnInit() override;
    void OnUpdate() override;
    void OnRender() override;
    void OnDestroy() override;
    void OnResize(UINT width, UINT height) override;

private:
    static const UINT FrameCount = 2;

    D3D12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize;
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;
    bool m_isMinimized;

    void LoadPipeline();
    void PopulateCommandList();
    void WaitForPreviousFrame();
    void Resize(UINT width, UINT height);
};