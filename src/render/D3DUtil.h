#pragma once

#include "stdafx.h"
#include "app/DXSampleHelper.h"
// Small helpers for the D3D12 boilerplate every renderer repeats.


namespace D3DUtil
{
    using Microsoft::WRL::ComPtr;

    // A committed buffer resource. UPLOAD heaps must start in GENERIC_READ; DEFAULT heaps usually in COMMON.
    inline ComPtr<ID3D12Resource> CreateBuffer(
        ID3D12Device* device,
        D3D12_HEAP_TYPE heapType,
        UINT64 byteSize,
        D3D12_RESOURCE_STATES initialState,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
    {
        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = heapType;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = byteSize;
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // buffers are always row major
        bufferDesc.Flags = flags;

        ComPtr<ID3D12Resource> buffer;
        ThrowIfFailed(device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            initialState,
            nullptr,
            IID_PPV_ARGS(&buffer)));
        return buffer;
    }

    // Compiles an HLSL file; errors go to the debugger output before throwing.
    inline ComPtr<ID3DBlob> CompileShaderFile(const std::wstring& path, LPCSTR entryPoint, LPCSTR target)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION; // readable shaders in PIX / graphics debuggers
#endif
        ComPtr<ID3DBlob> shader;
        ComPtr<ID3DBlob> errors;
        const HRESULT hr = D3DCompileFromFile(
            path.c_str(),
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entryPoint,
            target,
            flags,
            0,
            &shader,
            &errors);
        if (FAILED(hr))
        {
            if (errors != nullptr)
            {
                OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
            }
            ThrowIfFailed(hr);
        }
        return shader;
    }

    // Serializes and creates a root signature; serialization errors go to the debugger output.
    inline ComPtr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* device, const D3D12_ROOT_SIGNATURE_DESC& desc)
    {
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> errors;
        const HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
        if (FAILED(hr))
        {
            if (errors != nullptr)
            {
                OutputDebugStringA(static_cast<const char*>(errors->GetBufferPointer()));
            }
            ThrowIfFailed(hr);
        }

        ComPtr<ID3D12RootSignature> rootSignature;
        ThrowIfFailed(device->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature)));
        return rootSignature;
    }

    // Blending off, all color channels written: what every opaque pass wants.
    inline D3D12_BLEND_DESC OpaqueBlendDesc()
    {
        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC opaque =
        {
            FALSE, FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL
        };
        for (auto& renderTarget : blendDesc.RenderTarget)
        {
            renderTarget = opaque;
        }
        return blendDesc;
    }
}
