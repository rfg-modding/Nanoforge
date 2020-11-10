#pragma once
#include "common/Typedefs.h"
#include <d3d11.h>
#include <filesystem>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

//Wraps ID3D11Texture2D and provides helpers to create various views bound to the texture
class Texture2D
{
public:
    //Create D3D11 texture from provided parameters. If called more than once it will be recreated
    void Create(ID3D11Device* d3d11Device, u32 width, u32 height, DXGI_FORMAT format, D3D11_BIND_FLAG bindFlags, D3D11_SUBRESOURCE_DATA* data = nullptr);
    //Create render target view from texture. Must call ::Create first
    void CreateRenderTargetView();
    //Create shader resource view from texture. Must call ::Create first
    void CreateShaderResourceView();
    //Create depth stencil view from texture. Must call ::Create first
    void CreateDepthStencilView();

    //Get underlying raw pointer to ID3D11Texture2D. Should prefer adding member functions that access this if possible
    ID3D11Texture2D* Get() { return texture_.Get(); }
    //Get underlying raw pointer for render target view. Returns nullptr if one wasn't created
    ID3D11RenderTargetView* GetRenderTargetView() { return renderTargetView_.Get(); }
    //Gets pointer to the render target view pointer. 
    ID3D11RenderTargetView** GetRenderTargetViewPP() { return renderTargetView_.GetAddressOf(); }
    //Get underlying raw pointer for shader resource view. Returns nullptr if one wasn't created
    ID3D11ShaderResourceView* GetShaderResourceView() { return shaderResourceView_.Get(); }
    //Get underlying raw pointer for depth stencil view. Returns nullptr if one wasn't created
    ID3D11DepthStencilView* GetDepthStencilView() { return depthStencilView_.Get(); }

private:
    ComPtr<ID3D11Texture2D> texture_ = nullptr;
    ComPtr<ID3D11RenderTargetView> renderTargetView_ = nullptr;
    ComPtr<ID3D11ShaderResourceView> shaderResourceView_ = nullptr;
    ComPtr<ID3D11DepthStencilView> depthStencilView_ = nullptr;
    ID3D11Device* d3d11Device_ = nullptr;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
};