using Common;
using System;
using RfgTools.Formats.Meshes;
using Direct3D;

namespace Nanoforge.Render.Resources
{
	public class Mesh
	{
        public u32 NumLods { get; private set; } = 0;
        public u32 LodLevel { get; set; } = 0;

        //TODO: Change this to be shareable between mesh instances. Once chunk meshes are added having separate copies of this for each RenderObject will be a serious memory hog (was a problem in the C++ version)
        private MeshDataBlock _config = null ~DeleteIfSet!(_);

        private append Buffer _vertexBuffer;
        private append Buffer _indexBuffer;
        private u32 _numVertices = 0;
        private u32 _numIndices = 0;
        private u32 _vertexStride = 0;
        private u32 _indexStride = 0;
        private DXGI_FORMAT _indexBufferFormat = .UNKNOWN;
        private D3D_PRIMITIVE_TOPOLOGY _topology = .D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

        public Result<void> Init(ID3D11Device* device, MeshDataBlock config, Span<u8> indexBuffer, Span<u8> vertexBuffer, u32 numLods = 1)
		{
            _config = config.Clone(.. new .());

            //Test the current assumption that each lod level has only 1 submesh when multiple lods exist
            if (numLods != 1)
                Runtime.Assert(numLods == config.Header.NumSubmeshes);

            _vertexStride = config.Header.VertexStride0;
            _indexStride = config.Header.IndexSize;
            _numVertices = config.Header.NumVertices;
            _numIndices = config.Header.NumIndices;
            if (config.Header.IndexSize == 2)
                _indexBufferFormat = .R16_UINT;
            else if (config.Header.IndexSize == 4)
                _indexBufferFormat = .R32_UINT;
            else
                return .Err; //Unsupported index format

            _topology = .D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; //Hardcoded since all RFG meshes have used this so far
            NumLods = numLods;

            if (_indexBuffer.Init(device, (u32)indexBuffer.Length, .INDEX_BUFFER, indexBuffer.Ptr) case .Err)
                return .Err;
            if (_vertexBuffer.Init(device, (u32)vertexBuffer.Length, .VERTEX_BUFFER, vertexBuffer.Ptr) case .Err)
	            return .Err;

            return .Ok;
        }

        public void Draw(ID3D11DeviceContext* context)
        {
            //Bind buffers
            u32 vertexOffset = 0;
            context.IASetIndexBuffer(_indexBuffer.Ptr, _indexBufferFormat, 0);
            context.IASetVertexBuffers(0, 1, &_vertexBuffer.Ptr, &_vertexStride, &vertexOffset);

            if (NumLods == 1) //In this case the mesh is split into several submeshes
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