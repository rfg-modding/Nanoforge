using Common;
using System;
using RfgTools.Formats.Meshes;
using Direct3D;
using Nanoforge.Rfg;
using System.Collections;
using Nanoforge.Misc;

namespace Nanoforge.Render.Resources
{
	public class Mesh
	{
        public u32 NumLods { get; private set; } = 0;
        public u32 LodLevel { get; set; } = 0;

        private ProjectMesh _config = null;
        private append Buffer _vertexBuffer;
        private append Buffer _indexBuffer;
        private DXGI_FORMAT _indexBufferFormat = .UNKNOWN;

        public static u16[256] SubmeshOverrideIndices = .();

        public Result<void> Init(ID3D11Device* device, ProjectMesh config, u32 numLods = 1)
		{
            _config = config;

            //Test the current assumption that each lod level has only 1 submesh when multiple lods exist
            if (numLods != 1)
                Runtime.Assert(numLods == _config.Submeshes.Count);

            switch (config.IndexSize)
            {
                case 2:
                    _indexBufferFormat = .R16_UINT;
                case 4:
                    _indexBufferFormat = .R32_UINT;
                default:
                    Logger.Error("Failed to initialize mesh with unsupported index size of {} bytes.", config.IndexSize);
                    return .Err;
            }

            NumLods = numLods;

            //Load index + vertex buffers from file into RAM
            List<u8> indexBuffer = null;
            List<u8> vertexBuffer = null;
            defer { DeleteIfSet!(indexBuffer); }
            defer { DeleteIfSet!(vertexBuffer); }
            if (_config.IndexBuffer.Load() case .Ok(let val))
            {
            	indexBuffer = val;
            }
            else
            {
                Logger.Error("Failed to load index buffer for mesh {}", _config.Name);
                return .Err;
            }
            if (_config.VertexBuffer.Load() case .Ok(let val))
            {
            	vertexBuffer = val;
            }
            else
            {
                Logger.Error("Failed to load vertex buffer for mesh {}", _config.Name);
                return .Err;
            }

            //Load buffers into GPU memory
            if (_indexBuffer.Init(device, (u32)indexBuffer.Count, .INDEX_BUFFER, indexBuffer.Ptr) case .Err)
                return .Err;
            if (_vertexBuffer.Init(device, (u32)vertexBuffer.Count, .VERTEX_BUFFER, vertexBuffer.Ptr) case .Err)
	            return .Err;

            return .Ok;
        }

        public void Draw(ID3D11DeviceContext* context, int numSubmeshOverrides = -1)
        {
            //Bind buffers
            u32 vertexOffset = 0;
            u32 vertexStride = _config.VertexStride;
            context.IASetIndexBuffer(_indexBuffer.Ptr, _indexBufferFormat, 0);
            context.IASetVertexBuffers(0, 1, &_vertexBuffer.Ptr, &vertexStride, &vertexOffset);

            if (numSubmeshOverrides != -1)
            {
                //Special case used by RenderChunk to draw specific submeshes
                for (int i in 0 ..< numSubmeshOverrides)
                {
                    u32 submeshIndex = SubmeshOverrideIndices[i];
                    ref SubmeshData submesh = ref _config.Submeshes[submeshIndex];
                    u32 firstBlock = submesh.RenderBlocksOffset;
                    for (int i in 0 ..< submesh.NumRenderBlocks)
                    {
                        ref RenderBlock block = ref _config.RenderBlocks[firstBlock + i];
                        context.DrawIndexed(block.NumIndices, block.StartIndex, 0);
                    }
                }
            }
            else if (NumLods == 1) //In this case the mesh is split into several submeshes
            {
                for (var submesh in ref _config.Submeshes)
                {
                    u32 firstBlock = submesh.RenderBlocksOffset;
                    for (u32 i in 0 ..< submesh.NumRenderBlocks)
                    {
                        var block = ref _config.RenderBlocks[firstBlock + i];
                        context.DrawIndexed(block.NumIndices, block.StartIndex, 0);
                    }    
                }
            }
            else //In this case each submesh is a different lod level, so we only draw one submesh
            {
                var submesh = ref _config.Submeshes[LodLevel];
                u32 firstBlock = submesh.RenderBlocksOffset;
                for (u32 i in 0 ..< submesh.NumRenderBlocks)
                {
                    var block = ref _config.RenderBlocks[firstBlock + i];
                    context.DrawIndexed(block.NumIndices, block.StartIndex, 0);
                }
            }
        }
	}
}