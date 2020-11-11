#include "Buffer.h"
#include "Log.h"

void Buffer::Create(ComPtr<ID3D11Device> d3d11Device, u32 size, UINT bindFlags, UINT usage, UINT cpuAccessFlags, UINT miscFlags)
{
    //Save buffer size and reset existing data
    size_ = size;
    buffer_.Reset();

    //Fill out buffer description
    D3D11_BUFFER_DESC bufferDesc;
    ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));
    bufferDesc.Usage = (D3D11_USAGE)usage;
    bufferDesc.ByteWidth = size;
    bufferDesc.BindFlags = bindFlags;
    bufferDesc.CPUAccessFlags = cpuAccessFlags;
    bufferDesc.MiscFlags = miscFlags;

    //Attempt to create buffer
    if (FAILED(d3d11Device->CreateBuffer(&bufferDesc, nullptr, buffer_.GetAddressOf())))
        THROW_EXCEPTION("Failed to buffer in Buffer::Create()");
}

void Buffer::SetData(ComPtr<ID3D11DeviceContext> d3d11Context, void* pData)
{
    //Attempt to update buffer data from pData
    d3d11Context->UpdateSubresource(buffer_.Get(), 0, NULL, pData, 0, 0);
}