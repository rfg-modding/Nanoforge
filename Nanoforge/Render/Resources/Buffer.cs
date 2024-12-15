using System;
using Silk.NET.OpenGL;

namespace Nanoforge.Render.Resources
{
    public class Buffer : IDisposable
    {
        private uint _handle;
        private BufferTargetARB _bufferType;
        private GL _gl;

        public uint Handle => _handle;
        
        public unsafe Buffer(GL gl, Span<byte> data, BufferTargetARB bufferType)
        {
            _gl = gl;
            _bufferType = bufferType;

            _handle = _gl.GenBuffer();
            Bind();
            fixed (void* d = data)
            {
                _gl.BufferData(bufferType, (nuint)data.Length, d, BufferUsageARB.StaticDraw);
            }
        }

        public void Bind()
        {
            _gl.BindBuffer(_bufferType, _handle);
        }

        public void Dispose()
        {
            _gl.DeleteBuffer(_handle);
        }
    }
}
