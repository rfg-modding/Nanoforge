using Direct3D;
using Common;
using System;
using Nanoforge.Misc;

namespace Nanoforge.Render.Resources
{
    //Wrapper for D3D11 2D textures
	public class Texture2D
	{
        public append String Name;
        private ID3D11Device* _device = null;
        private ID3D11Texture2D* _texture = null ~ReleaseCOM(&_);
        private ID3D11RenderTargetView* _renderTargetView = null ~ReleaseCOM(&_);
        private ID3D11ShaderResourceView* _shaderResourceView = null ~ReleaseCOM(&_);
        private ID3D11DepthStencilView* _depthStencilView = null ~ReleaseCOM(&_);
        private ID3D11SamplerState* _samplerState = null ~ReleaseCOM(&_);
        private DXGI_FORMAT _format;
        private u32 _mipLevels = 1;

        public ID3D11Texture2D* Value => _texture;
        public ID3D11RenderTargetView* RenderTargetView => _renderTargetView;
        public ID3D11RenderTargetView** RenderTargetViewPointer => &_renderTargetView;
        public ID3D11ShaderResourceView* ShaderResourceView => _shaderResourceView;
        public ID3D11DepthStencilView* DepthStencilView => _depthStencilView;

        public this(StringView name = "")
        {
            Name.Set(name);
        }

        public Result<void> Init(ID3D11Device* device, u32 width, u32 height, DXGI_FORMAT format, D3D11_BIND_FLAG bindFlags, D3D11_SUBRESOURCE_DATA* data = null, u32 mipLevels = 1)
        {
            _device = device;
            _format = format;
            _mipLevels = mipLevels;

            ReleaseCOM(&_depthStencilView);
            ReleaseCOM(&_shaderResourceView);
            ReleaseCOM(&_renderTargetView);
            ReleaseCOM(&_texture);

            D3D11_TEXTURE2D_DESC desc;
            ZeroMemory(&desc);
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = _mipLevels;
            desc.ArraySize = 1;
            desc.Format = _format;
            desc.SampleDesc.Count = 1;
            desc.Usage = .DEFAULT;
            desc.BindFlags = bindFlags;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;
            i32 createResult = device.CreateTexture2D(&desc, data, &_texture);
            if (FAILED!(createResult))
            {
                Logger.Error("Failed to create D3D11 Texture2D. Error code: {}", createResult);
                return .Err;
            }

#if DEBUG
            _texture.SetDebugName(Name);
#endif
            return .Ok;
        }

        public void Bind(ID3D11DeviceContext* context, u32 index)
        {
            context.PSSetShaderResources(index, 1, &_shaderResourceView);
            context.PSSetSamplers(index, 1, &_samplerState);
        }

        public void CreateRenderTargetView()
        {
            //Release pre-existing view
            ReleaseCOM(&_renderTargetView);

            D3D11_RENDER_TARGET_VIEW_DESC desc;
            ZeroMemory(&desc);
            desc.Format = _format;
            desc.ViewDimension = .TEXTURE2D;
            desc.Texture2D.MipSlice = 0;

            i32 createResult = _device.CreateRenderTargetView(_texture, &desc, &_renderTargetView);
            if (FAILED!(createResult))
            {
                Logger.Error("Failed to create render target view for D3D11 texture 2d. Error code: {}", createResult);
            }
        }

        public void CreateShaderResourceView()
        {
            ReleaseCOM(&_shaderResourceView);

            D3D11_SHADER_RESOURCE_VIEW_DESC desc;
            ZeroMemory(&desc);
            desc.Format = _format;
            desc.ViewDimension = .D3D11_SRV_DIMENSION_TEXTURE2D;
            desc.Texture2D.MostDetailedMip = 0;
            desc.Texture2D.MipLevels = _mipLevels;

            i32 createResult = _device.CreateShaderResourceView(_texture, &desc, &_shaderResourceView);
            if (FAILED!(createResult))
            {
                Logger.Error("Failed to create shader resource view for D3D11 texture 2d. Error code: {}", createResult);
            }
        }

        public void CreateDepthStencilView()
        {
            ReleaseCOM(&_depthStencilView);

            i32 createResult = _device.CreateDepthStencilView(_texture, null, &_depthStencilView);
            if (FAILED!(createResult))
            {
                Logger.Error("Failed to create depth stencil view for D3D11 texture 2d. Error code: {}", createResult);
            }
        }

        public void CreateSampler()
        {
            D3D11_SAMPLER_DESC desc;
            ZeroMemory(&desc);
            desc.Filter = .MIN_MAG_MIP_LINEAR;
            desc.AddressU = .WRAP;
            desc.AddressV = .WRAP;
            desc.AddressW = .WRAP;
            desc.ComparisonFunc = .NEVER;
            desc.MinLOD = 0;
            desc.MaxLOD = f32.MaxValue;

            i32 createResult = _device.CreateSamplerState(&desc, &_samplerState);
            if (FAILED!(createResult))
            {
                Logger.Error("Failed to create sampler for D3D11 texture 2d. Error code: {}", createResult);
            }
        }

        public void SetDebugName(StringView name)
        {
            if (_texture != null)
                _texture.SetDebugName(name);
        }
	}
}