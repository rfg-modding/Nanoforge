using System;
using System.Collections.Generic;
using RFGM.Formats.Meshes.Shared;
using Serilog;
using Silk.NET.OpenGL;

namespace Nanoforge.Render.Resources
{
    public class Mesh : IDisposable
    {
        public Material Material;
        private uint _vao;
        public Buffer VertexBuffer;
        public Buffer IndexBuffer;
        public MeshConfig Config;
        public GL GL { get; }
        
        public Mesh(GL gl, MeshInstanceData meshData, Material material)
        {
            GL = gl;
            VertexBuffer = new(gl, meshData.Vertices, BufferTargetARB.ArrayBuffer);
            IndexBuffer = new(gl, meshData.Indices, BufferTargetARB.ElementArrayBuffer);
            Config = meshData.Config;
            Material = material;
            _vao = material.MakeVAO(gl, VertexBuffer.Handle, IndexBuffer.Handle);
        }
        
        public unsafe void Draw()
        {
            DrawElementsType elementType = Config.IndexSize switch
            {
                1 => DrawElementsType.UnsignedByte,
                2 => DrawElementsType.UnsignedShort,
                4 => DrawElementsType.UnsignedInt,
                _ => throw new ArgumentOutOfRangeException()
            };

            foreach (var submesh in Config.Submeshes)
            {
                int firstBlock = (int)submesh.RenderBlocksOffset;
                for (int i = 0; i < submesh.NumRenderBlocks; i++)
                {
                    var block = Config.RenderBlocks[firstBlock + i];
                    GL.DrawElements(PrimitiveType.TriangleStrip, block.NumIndices, elementType, (void*)(block.StartIndex));
                }
            }
        }

        public void Bind()
        {
            GL.BindVertexArray(_vao);
        }

        public void Dispose()
        {
            GL.DeleteVertexArray(_vao);
            VertexBuffer.Dispose();
            IndexBuffer.Dispose();
        }
    }
}
