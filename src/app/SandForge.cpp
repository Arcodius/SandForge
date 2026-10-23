#include "stdafx.h"
#include "SandForge.h"

SandForge::SandForge(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_scissorRect{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) },
    m_rtvDescriptorSize(0),
    m_frameIndex(0),
    m_fenceEvent(nullptr),
    m_fenceValue(0),
    m_isMinimized(false)
{
}

void SandForge::OnInit()
{
    LoadPipeline();
}

void SandForge::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);
        if (hardwareAdapter == nullptr)
        {
            ThrowIfFailed(E_FAIL);
        }

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)));
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    RECT clientRect = {};
    if (!GetClientRect(Win32Application::GetHwnd(), &clientRect))
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
    m_width = static_cast<UINT>(clientRect.right - clientRect.left);
    m_height = static_cast<UINT>(clientRect.bottom - clientRect.top);
    m_isMinimized = (m_width == 0 || m_height == 0);
    m_aspectRatio = m_height == 0 ? 0.0f : static_cast<float>(m_width) / m_height;

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain));
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < FrameCount; ++i)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescriptorSize;
    }

    ThrowIfFailed(m_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_commandAllocator)));
    ThrowIfFailed(m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&m_commandList)));
    ThrowIfFailed(m_commandList->Close());

    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    m_scissorRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
}

void SandForge::OnUpdate()
{
}

void SandForge::OnRender()
{
    if (m_isMinimized || m_swapChain == nullptr)
    {
        return;
    }

    PopulateCommandList();

    ID3D12CommandList* commandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);
    ThrowIfFailed(m_swapChain->Present(1, 0));
    WaitForPreviousFrame();
}

void SandForge::PopulateCommandList()
{
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    const D3D12_VIEWPORT viewport = {
        0.0f,
        0.0f,
        static_cast<float>(m_width),
        static_cast<float>(m_height),
        0.0f,
        1.0f
    };
    m_commandList->RSSetViewports(1, &viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(m_frameIndex) * m_rtvDescriptorSize;
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const float canvasColor[] = { 0.08f, 0.09f, 0.11f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, canvasColor, 0, nullptr);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);
    ThrowIfFailed(m_commandList->Close());
}

void SandForge::WaitForPreviousFrame()
{
    const UINT64 fenceValue = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fenceValue));
    ++m_fenceValue;

    if (m_fence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void SandForge::OnResize(UINT width, UINT height)
{
    m_isMinimized = (width == 0 || height == 0);
    if (m_isMinimized)
    {
        return;
    }

    if (m_swapChain == nullptr)
    {
        m_width = width;
        m_height = height;
        m_aspectRatio = static_cast<float>(width) / height;
        m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
        return;
    }

    Resize(width, height);
}

void SandForge::Resize(UINT width, UINT height)
{
    if (width == m_width && height == m_height)
    {
        return;
    }

    WaitForPreviousFrame();
    for (auto& renderTarget : m_renderTargets)
    {
        renderTarget.Reset();
    }

    ThrowIfFailed(m_swapChain->ResizeBuffers(
        FrameCount,
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        0));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < FrameCount; ++i)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtvDescriptorSize;
    }

    m_width = width;
    m_height = height;
    m_aspectRatio = static_cast<float>(width) / height;
    m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
}

void SandForge::OnDestroy()
{
    if (m_fence != nullptr && m_commandQueue != nullptr && m_swapChain != nullptr)
    {
        WaitForPreviousFrame();
    }
    if (m_fenceEvent != nullptr)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
}
