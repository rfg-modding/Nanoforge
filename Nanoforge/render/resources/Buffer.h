#pragma once
#include "common/Typedefs.h"
#include <d3d11.h>
#include <filesystem>
#include <span>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

//Wraps ID3D11Buffer
class Buffer
{
public:
    //Creates a buffer from the provided variables
    void Create(ComPtr<ID3D11Device> d3d11Device, u32 size, u32 bindFlags, void* data = nullptr, u32 usage = D3D11_USAGE_DEFAULT, u32 cpuAccessFlags = 0, u32 miscFlags = 0);
    //Update contents of buffer from pData
    void SetData(ComPtr<ID3D11DeviceContext> d3d11Context, void* pData);
    //Get raw pointer of underlying ID3D11Buffer
    ID3D11Buffer* Get() { return buffer_.Get(); }
    //Get pointer pointer to underlying ID3D11Buffer
    ID3D11Buffer** GetAddressOf() { return buffer_.GetAddressOf(); }
    //Recreate buffer with provided size
    void Resize(ComPtr<ID3D11Device> d3d11Device, u32 newSize);
    //Resize the buffer if its size is less than requiredSize
    void ResizeIfNeeded(ComPtr<ID3D11Device> d3d11Device, u32 requiredSize);

private:
    ComPtr<ID3D11Buffer> buffer_ = nullptr;
    u32 size_ = 0;
    u32 bindFlags_ = 0;
    u32 usage_ = 0;
    u32 cpuAccessFlags_ = 0;
    u32 miscFlags_ = 0;
};