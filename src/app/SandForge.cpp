#include "stdafx.h"
#include "SandForge.h"

#include "render/D3DUtil.h"

#include <random>

SandForge::SandForge(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_scissorRect{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) },
    m_mappedCameraConstants(nullptr),
    m_rtvDescriptorSize(0),
    m_frameIndex(0),
    m_fenceEvent(nullptr),
    m_fenceValue(0),
    m_isMinimized(false),
    m_keys{},
    m_looking(false),
    m_lastMouseX(0),
    m_lastMouseY(0),
    m_cameraPosition(0.0f, 0.0f, -3.0f),
    m_cameraYaw(0.0f),
    m_cameraPitch(0.0f),
    m_fieldOfView(DirectX::XM_PIDIV4),
    m_previousUpdate(std::chrono::steady_clock::now())
{
}

void SandForge::OnInit()
{
    LoadPipeline();
    LoadAssets();
    LoadParticles();
    m_particleRenderer.Init(m_device.Get(), static_cast<UINT>(m_particles.size()));
    m_particleRenderer.Upload(m_particles.data(), static_cast<UINT>(m_particles.size()));
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

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));
    CreateDepthBuffer(m_width, m_height);

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

void SandForge::LoadAssets()
{
    LoadCubeMesh();

    // Full paths: relative ones resolve against the working directory, which isn't the exe folder under the VS debugger
    ComPtr<ID3DBlob> vertexShader = D3DUtil::CompileShaderFile(GetAssetFullPath(L"shaders\\cube_vs.hlsl"), "VSMain", "vs_5_0");
    ComPtr<ID3DBlob> pixelShader = D3DUtil::CompileShaderFile(GetAssetFullPath(L"shaders\\cube_ps.hlsl"), "PSMain", "ps_5_0");

    D3D12_ROOT_PARAMETER rootParameter = {};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameter.Descriptor.ShaderRegister = 0;
    rootParameter.Descriptor.RegisterSpace = 0;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 1;
    rootSignatureDesc.pParameters = &rootParameter;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    m_rootSignature = D3DUtil::CreateRootSignature(
        m_device.Get(), rootSignatureDesc
    );

    const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
    pipelineDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    pipelineDesc.pRootSignature = m_rootSignature.Get();
    pipelineDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() }; // shader in bytes
    pipelineDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // no back culling
    pipelineDesc.RasterizerState.DepthClipEnable = TRUE;
    pipelineDesc.BlendState = D3DUtil::OpaqueBlendDesc();
    pipelineDesc.DepthStencilState.DepthEnable = TRUE; // enable depth test
    pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pipelineDesc.DepthStencilState.StencilEnable = FALSE;
    pipelineDesc.SampleMask = UINT_MAX;
    pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // draw triangle
    pipelineDesc.NumRenderTargets = 1;
    pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // Keep the same format as swapchain
    pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pipelineDesc.SampleDesc.Count = 1;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState( // create PSO
        &pipelineDesc,
        IID_PPV_ARGS(&m_pipelineState)));
    
    m_vertexBuffer = D3DUtil::CreateBuffer(
        m_device.Get(), D3D12_HEAP_TYPE_UPLOAD, m_cubeVertices.size() * sizeof(Vertex), D3D12_RESOURCE_STATE_GENERIC_READ
    );

    void* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, 0 }; // Indicates CPU does not read the memory
    ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, &mappedData)); // Get CPU pointer
    memcpy(mappedData, m_cubeVertices.data(), m_cubeVertices.size() * sizeof(Vertex));
    m_vertexBuffer->Unmap(0, nullptr);
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = static_cast<UINT>(m_cubeVertices.size() * sizeof(Vertex));

    m_indexBuffer = D3DUtil::CreateBuffer(
        m_device.Get(), D3D12_HEAP_TYPE_UPLOAD, m_cubeIndices.size() * sizeof(UINT32), D3D12_RESOURCE_STATE_GENERIC_READ
    );
    ThrowIfFailed(m_indexBuffer->Map(0, &readRange, &mappedData));
    memcpy(mappedData, m_cubeIndices.data(), m_cubeIndices.size() * sizeof(UINT32));
    m_indexBuffer->Unmap(0, nullptr);

    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    m_indexBufferView.SizeInBytes = static_cast<UINT>(m_cubeIndices.size() * sizeof(UINT32));

    m_cameraBuffer = D3DUtil::CreateBuffer(
        m_device.Get(), D3D12_HEAP_TYPE_UPLOAD, sizeof(CameraConstants),
        D3D12_RESOURCE_STATE_GENERIC_READ
    );
    ThrowIfFailed(m_cameraBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedCameraConstants)));
    UpdateCameraConstants();
}

void SandForge::LoadCubeMesh()
{
    const std::wstring meshPath = GetAssetFullPath(L"assets\\cube.obj");
    std::ifstream meshFile(meshPath);
    if (!meshFile)
    {
        throw std::runtime_error("Failed to open assets/cube.obj");
    }

    std::vector<Vertex> positions;
    std::string line;
    UINT lineNumber = 0;
    while (std::getline(meshFile, line))
    {
        ++lineNumber;
        std::istringstream stream(line);
        std::string command;
        stream >> command;
        if (command.empty() || command[0] == '#')
        {
            continue;
        }

        if (command == "v")
        {
            Vertex vertex = {};
            if (!(stream >> vertex.position.x >> vertex.position.y >> vertex.position.z
                >> vertex.color.x >> vertex.color.y >> vertex.color.z))
            {
                throw std::runtime_error("Invalid vertex in assets/cube.obj at line " + std::to_string(lineNumber));
            }
            vertex.color.w = 1.0f;
            positions.push_back(vertex);
        }
        else if (command == "f")
        {
            std::vector<UINT32> face;
            std::string token;
            while (stream >> token)
            {
                const size_t slash = token.find('/');
                const std::string indexText = token.substr(0, slash);
                const int objIndex = std::stoi(indexText);
                const int resolvedIndex = objIndex > 0
                    ? objIndex - 1
                    : static_cast<int>(positions.size()) + objIndex;
                if (resolvedIndex < 0 || static_cast<size_t>(resolvedIndex) >= positions.size())
                {
                    throw std::runtime_error("Invalid face index in assets/cube.obj at line " + std::to_string(lineNumber));
                }
                face.push_back(static_cast<UINT32>(resolvedIndex));
            }
            if (face.size() < 3)
            {
                throw std::runtime_error("Face has fewer than three vertices in assets/cube.obj at line " + std::to_string(lineNumber));
            }

            const UINT32 firstVertex = static_cast<UINT32>(m_cubeVertices.size());
            for (const UINT32 index : face)
            {
                m_cubeVertices.push_back(positions[index]);
            }
            for (UINT32 i = 1; i + 1 < face.size(); ++i)
            {
                m_cubeIndices.push_back(firstVertex);
                m_cubeIndices.push_back(firstVertex + i);
                m_cubeIndices.push_back(firstVertex + i + 1);
            }
        }
    }

    if (m_cubeVertices.empty() || m_cubeIndices.empty())
    {
        throw std::runtime_error("assets/cube.obj contains no renderable mesh");
    }
}

void SandForge::LoadParticles() {
    constexpr size_t w = 32;
    constexpr float spacing = 0.4f / w;
    constexpr size_t pNum = w*w*w;

    // Mersenne Twister, fixed seed: every run starts from the same state, so bugs reproduce
    std::mt19937 gen(12345);
    // +-25% of the spacing: breaks the regular lattice without letting neighbours overlap
    std::uniform_real_distribution<float> dist_coord(-0.25f * spacing, 0.25f * spacing);
    std::uniform_real_distribution<float> dist_shade(0.75f, 1.25f);

    m_particles.reserve(pNum);
    for (size_t z = 0; z < w; ++z) {
        float posZ = 0.3f + (z+0.5f) * spacing;
        for (size_t y = 0; y < w; ++y) {
            float posY = 0.5f + (y+0.5f) * spacing;
            for (size_t x = 0; x < w; ++x) {
                float posX = 0.3f + (x+0.5f) * spacing;
                m_particles.push_back(Particle{
                    DirectX::XMFLOAT3(posX+dist_coord(gen), posY+dist_coord(gen), posZ+dist_coord(gen)),
                    dist_shade(gen)
                });
            }
        }
    }
}

void SandForge::UpdateCameraConstants()
{
    const DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&m_cameraPosition);
    const float cosPitch = cosf(m_cameraPitch);
    const DirectX::XMVECTOR forward = DirectX::XMVectorSet(
        sinf(m_cameraYaw) * cosPitch,
        sinf(m_cameraPitch),
        cosf(m_cameraYaw) * cosPitch,
        0.0f);
    const DirectX::XMVECTOR target = DirectX::XMVectorAdd(position, forward);
    const DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(
        position,
        target,
        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(
        m_fieldOfView,
        m_aspectRatio,
        0.1f,
        100.0f);
    DirectX::XMStoreFloat4x4(
        &m_mappedCameraConstants->viewProjection,
        view * projection);
}

void SandForge::CreateDepthBuffer(UINT width, UINT height)
{
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // default heap: specific for GPU

    // A depth buffer is a TEXTURE2D, not a buffer: Width/Height are pixels, it has a format,
    // a driver-chosen layout and an optimized clear value. D3DUtil::CreateBuffer can't describe any of that.
    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&m_depthBuffer)));
    m_device->CreateDepthStencilView(
        m_depthBuffer.Get(),
        nullptr,
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void SandForge::OnUpdate()
{
    const auto now = std::chrono::steady_clock::now();
    const float deltaTime = (std::min)(
        std::chrono::duration<float>(now - m_previousUpdate).count(),
        0.1f);
    m_previousUpdate = now;
    m_accumulator += deltaTime;
    int steps = 0;
    while (m_accumulator >= TickSeconds && steps < MaxStepsPerFrame) {
        m_accumulator -= TickSeconds;
        ++steps;
    }
    if (steps == MaxStepsPerFrame) m_accumulator = 0.0f;

    if (deltaTime <= 0.0f || m_isMinimized)
    {
        return;
    }

    if (!m_paused)
    {
        for (int i = 0; i < steps; ++i)
        {
            // Simulation tick goes here (M1 task 5: placeholder particle physics)
        }
    }
    m_particleRenderer.Upload(m_particles.data(), static_cast<UINT>(m_particles.size()));

    const float forwardX = sinf(m_cameraYaw);
    const float forwardZ = cosf(m_cameraYaw);
    float moveX = 0.0f;
    float moveY = 0.0f;
    float moveZ = 0.0f;

    if (m_keys['W'])
    {
        moveX += forwardX;
        moveZ += forwardZ;
    }
    if (m_keys['S'])
    {
        moveX -= forwardX;
        moveZ -= forwardZ;
    }
    if (m_keys['D'])
    {
        moveX += forwardZ;
        moveZ -= forwardX;
    }
    if (m_keys['A'])
    {
        moveX -= forwardZ;
        moveZ += forwardX;
    }
    if (m_keys['E'])
    {
        moveY += 1.0f;
    }
    if (m_keys['Q'])
    {
        moveY -= 1.0f;
    }
    const float moveLength = sqrtf(moveX * moveX + moveY * moveY + moveZ * moveZ);
    if (moveLength > 0.0f)
    {
        constexpr float moveSpeed = 3.0f;
        const float distance = moveSpeed * deltaTime / moveLength;
        m_cameraPosition.x += moveX * distance;
        m_cameraPosition.y += moveY * distance;
        m_cameraPosition.z += moveZ * distance;
    }

    UpdateCameraConstants();
}

void SandForge::OnKeyDown(UINT8 key)
{
    const bool firstPress = !m_keys[key];
    m_keys[key] = true;

    if (key == 'P' && firstPress) m_paused = !m_paused;
}

void SandForge::OnKeyUp(UINT8 key)
{
    m_keys[key] = false;
}

void SandForge::OnMouseMove(int x, int y)
{
    if (m_looking)
    {
        constexpr float radiansPerPixel = 0.004f;
        m_cameraYaw += static_cast<float>(x - m_lastMouseX) * radiansPerPixel;
        m_cameraPitch -= static_cast<float>(y - m_lastMouseY) * radiansPerPixel;
        constexpr float pitchLimit = DirectX::XM_PIDIV2 - 0.01f;
        m_cameraPitch = (std::clamp)(m_cameraPitch, -pitchLimit, pitchLimit);
    }
    m_lastMouseX = x;
    m_lastMouseY = y;
}

void SandForge::OnMouseButton(MouseButton button, bool pressed, int x, int y)
{
    if (button == MouseButton::Right)
    {
        m_looking = pressed;
        m_lastMouseX = x;
        m_lastMouseY = y;
    }
}

void SandForge::OnMouseWheel(short delta)
{
    constexpr float radiansPerNotch = DirectX::XM_PI / 36.0f;
    m_fieldOfView -= static_cast<float>(delta) / WHEEL_DELTA * radiansPerNotch;
    m_fieldOfView = (std::clamp)(
        m_fieldOfView,
        DirectX::XM_PI / 9.0f,
        DirectX::XM_PI / 1.8f);
    UpdateCameraConstants();
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

// Per-frame skeleton: reset -> back buffer to RENDER_TARGET -> draw -> back to PRESENT -> close.
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

    RecordScene();

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);
    ThrowIfFailed(m_commandList->Close());
}

void SandForge::RecordScene()
{
    // Get the current handle by frame index
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(m_frameIndex) * m_rtvDescriptorSize;
    const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float canvasColor[] = { 0.08f, 0.09f, 0.11f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, canvasColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f, // farthest depth
        0,
        0,
        nullptr);

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->SetGraphicsRootConstantBufferView(
        0,
        m_cameraBuffer->GetGPUVirtualAddress());
    m_commandList->SetPipelineState(m_pipelineState.Get());
    // Tell Input Assembler (IA) how to read data
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);
    m_commandList->DrawIndexedInstanced(
        static_cast<UINT>(m_cubeIndices.size()),
        1,
        0,
        0,
        0);
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
    m_depthBuffer.Reset();

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
    CreateDepthBuffer(width, height);

    m_width = width;
    m_height = height;
    m_aspectRatio = static_cast<float>(width) / height;
    m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    UpdateCameraConstants();
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
    if (m_cameraBuffer != nullptr && m_mappedCameraConstants != nullptr)
    {
        m_cameraBuffer->Unmap(0, nullptr);
        m_mappedCameraConstants = nullptr;
    }
}
