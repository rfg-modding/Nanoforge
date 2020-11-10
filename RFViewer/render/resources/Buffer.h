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
    void Create(ComPtr<ID3D11Device> d3d11Device, u32 size, UINT bindFlags);
    //Update contents of buffer from pData
    void SetData(ComPtr<ID3D11DeviceContext> d3d11Context, void* pData);
    //Get raw pointer of underlying ID3D11Buffer
    ID3D11Buffer* Get() { return buffer_.Get(); }
    //Get pointer pointer to underlying ID3D11Buffer
    ID3D11Buffer** GetAddressOf() { return buffer_.GetAddressOf(); }

private:
    ComPtr<ID3D11Buffer> buffer_ = nullptr;
    u32 size_ = 0;
};