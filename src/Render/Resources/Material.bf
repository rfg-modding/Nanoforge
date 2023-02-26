using Nanoforge;
using Direct3D;
using System;
using Common;
using Nanoforge.Misc;

namespace Nanoforge.Render.Resources
{
    public class Material
    {
        public append String Name;
        private ID3D11InputLayout* _vertexLayout ~ReleaseCOM(&_);
        private append Shader _shader;
        public bool EnableGeometryShaders = false;

        public Result<void> Init(ID3D11Device* device, StringView shaderName, Span<D3D11_INPUT_ELEMENT_DESC> layout, bool enableGeometryShaders = false)
        {
            Name.Set(shaderName);

            //Compile shaders
            if (_shader.Load(shaderName, device, enableGeometryShaders) case .Err)
                return .Err;

            ID3DBlob* vsBlob = _shader.GetVertexShaderBlob();

            //Setup vertex input layout
            if (FAILED!(device.CreateInputLayout(layout.Ptr, (u32)layout.Length, vsBlob.GetBufferPointer(), vsBlob.GetBufferSize(), &_vertexLayout)))
            {
                Logger.Fatal("Failed to create vertex layout for material '{}'", shaderName);
                return .Err;
            }

            return .Ok;
        }

        public void Use(ID3D11DeviceContext* context)
        {
            context.IASetInputLayout(_vertexLayout);
            _shader.Bind(context);
        }

        public void TryReloadShaders()
        {
            _shader.TryReload();
        }
    }
}

