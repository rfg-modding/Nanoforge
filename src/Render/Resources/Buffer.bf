using Common;
using System;
using Direct3D;
using Nanoforge.Misc;

namespace Nanoforge.Render.Resources
{
	public class Buffer
	{
        public ID3D11Buffer* Ptr = null ~ReleaseCOM(&_);
        private u32 _size = 0;
        private D3D11_BIND_FLAG _bindFlags = 0;
        private D3D11_USAGE _usage = 0;
        private D3D11_CPU_ACCESS_FLAG _cpuAccessFlags = 0;
        private u32 _miscFlags = 0;

        public Result<void> Init(ID3D11Device* device, u32 size, D3D11_BIND_FLAG bindFlags, void* data = null, D3D11_USAGE usage = .DEFAULT, D3D11_CPU_ACCESS_FLAG cpuAccessFlags = 0, u32 miscFlags = 0)
        {
            _size = size;
            _bindFlags = bindFlags;
            _usage = usage;
            _cpuAccessFlags = cpuAccessFlags;
            _miscFlags = miscFlags;
            ReleaseCOM(&Ptr);

            //Fill buffer descriptor
            D3D11_BUFFER_DESC desc;
            ZeroMemory(&desc);
            desc.Usage = usage;
            desc.ByteWidth = size;
            desc.BindFlags = (u32)bindFlags;
            desc.CPUAccessFlags = (u32)cpuAccessFlags;
            desc.MiscFlags = miscFlags;

            D3D11_SUBRESOURCE_DATA initialData;
            ZeroMemory(&initialData);
            initialData.pSysMem = data;
            initialData.SysMemPitch = 0;
            initialData.SysMemSlicePitch = 0;

            //Attempt to create buffer
            i32 createResult = device.CreateBuffer(&desc, data != null ? &initialData : null, &Ptr);
            if (FAILED!(createResult))
            {
                Logger.Error("Failed to create D3D11 buffer. Error code: {}", createResult);
                return .Err;
            }

            return .Ok;
        }

        public void SetData(ID3D11DeviceContext* context, void* data)
        {
            context.UpdateSubresource(Ptr, 0, null, data, 0, 0);
        }

        public void Resize(ID3D11Device* device, u32 newSize)
        {
            Init(device, newSize, _bindFlags, null, _usage, _cpuAccessFlags, _miscFlags);
        }

        public void ResizeIfNeeded(ID3D11Device* device, u32 requiredSize)
        {
            if (_size < requiredSize)
                Resize(device, requiredSize);
        }
	}
}