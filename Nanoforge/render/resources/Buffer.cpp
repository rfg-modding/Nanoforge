#include "Buffer.h"
#include "Log.h"

void Buffer::Create(ComPtr<ID3D11Device> d3d11Device, u32 size, u32 bindFlags, void* data, u32 usage,  u32 cpuAccessFlags, u32 miscFlags)
{
    //Save buffer size and reset existing data
    size_ = size;
    bindFlags_ = bindFlags;
    usage_ = usage;
    cpuAccessFlags_ = cpuAccessFlags;
    miscFlags_ = miscFlags;
    buffer_.Reset();

    //Fill out buffer description
    D3D11_BUFFER_DESC bufferDesc;
    ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));
    bufferDesc.Usage = (D3D11_USAGE)usage;
    bufferDesc.ByteWidth = size;
    bufferDesc.BindFlags = bindFlags;
    bufferDesc.CPUAccessFlags = cpuAccessFlags;
    bufferDesc.MiscFlags = miscFlags;

    D3D11_SUBRESOURCE_DATA initialData;
    ZeroMemory(&initialData, sizeof(D3D11_SUBRESOURCE_DATA));
    initialData.pSysMem = data;
    initialData.SysMemPitch = 0;
    initialData.SysMemSlicePitch = 0;

    //Attempt to create buffer
    if (FAILED(d3d11Device->CreateBuffer(&bufferDesc, data ? &initialData : nullptr, buffer_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create render buffer.");
}

void Buffer::SetData(ComPtr<ID3D11DeviceContext> d3d11Context, void* pData)
{
    //Attempt to update buffer data from pData
    d3d11Context->UpdateSubresource(buffer_.Get(), 0, NULL, pData, 0, 0);
}

void Buffer::Resize(ComPtr<ID3D11Device> d3d11Device, u32 newSize)
{
    Create(d3d11Device, newSize, bindFlags_, nullptr, usage_, cpuAccessFlags_, miscFlags_);
}

void Buffer::ResizeIfNeeded(ComPtr<ID3D11Device> d3d11Device, u32 requiredSize)
{
    if (size_ < requiredSize)
        Resize(d3d11Device, requiredSize);
}
