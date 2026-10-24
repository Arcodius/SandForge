#pragma once

#include "DXSample.h"
#include <chrono>
#include <vector>

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
    void OnKeyDown(UINT8 key) override;
    void OnKeyUp(UINT8 key) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseButton(bool pressed, int x, int y) override;
    void OnMouseWheel(short delta) override;

private:
    static const UINT FrameCount = 2;
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    };

    struct alignas(256) CameraConstants
    {
        DirectX::XMFLOAT4X4 viewProjection;
        float padding[48];
    };

    enum class ViewMode { View2D, View3D };
    static constexpr UINT GridWidth = 320;
    static constexpr UINT GridHeight = 180;

    struct GridConstants {
        UINT gridWidth;
        UINT gridHeight;
        float viewportWidth;
        float viewportHeight;
    };

    D3D12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12Resource> m_depthBuffer;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12Resource> m_vertexBuffer; // D3D12Resource is only a COM object / handler, not memory itself
    ComPtr<ID3D12Resource> m_indexBuffer;
    ComPtr<ID3D12Resource> m_cameraBuffer;
    CameraConstants* m_mappedCameraConstants;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    UINT m_rtvDescriptorSize;
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;
    bool m_isMinimized;
    bool m_keys[256];
    bool m_looking;
    int m_lastMouseX;
    int m_lastMouseY;
    DirectX::XMFLOAT3 m_cameraPosition;
    float m_cameraYaw;
    float m_cameraPitch;
    float m_fieldOfView;
    std::chrono::steady_clock::time_point m_previousUpdate;
    std::vector<Vertex> m_cubeVertices;
    std::vector<UINT32> m_cubeIndices;

    ViewMode m_viewMode;
    ComPtr<ID3D12RootSignature> m_gridRootSignature;
    ComPtr<ID3D12PipelineState> m_gridPipelineState;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12Resource> m_gridBuffer;        // DEFAULT heap
    ComPtr<ID3D12Resource> m_gridUploadBuffer;  // UPLOAD heap
    UINT32* m_mappedGridUpload;
    std::vector<UINT32> m_cells;

    void LoadPipeline();
    void LoadAssets();
    void LoadCubeMesh();
    void UpdateCameraConstants();
    ComPtr<ID3DBlob> CompileShader(LPCWSTR assetName, LPCSTR entryPoint, LPCSTR target);
    void CreateDepthBuffer(UINT width, UINT height);
    void PopulateCommandList();
    void WaitForPreviousFrame();
    void Resize(UINT width, UINT height);

    void CreateGridResources();
    void LoadGridPipeline();
    void RecordGrid2D();
    void RecordCube3D();
    void UpdateGrid();
};