#include "Mesh.h"
#include "Log.h"

void Mesh::Create(ComPtr<ID3D11Device> d3d11Device, ComPtr<ID3D11DeviceContext> d3d11Context, MeshInstanceData data, u32 numLods)
{
    //All RFG meshes seen so far use these
    static const DXGI_FORMAT IndexBufferFormat = DXGI_FORMAT_R16_UINT;
    static const D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

    //Test the current assumption that each lod level has only 1 submesh when multiple lods exist
    if (numLods != 1)
        assert(numLods == data.Info.NumSubmeshes);

    //Data layout
    vertexStride_ = data.Info.VertexStride0;
    indexStride_ = data.Info.IndexSize;
    numVertices_ = data.Info.NumVertices;
    numIndices_ = data.Info.NumIndices;
    indexBufferFormat_ = IndexBufferFormat;
    topology_ = Topology;
    numLods_ = numLods;
    info_ = data.Info;

    //Create and set data for index and vertex buffers
    indexBuffer_.Create(d3d11Device, static_cast<u32>(data.IndexBuffer.size_bytes()), D3D11_BIND_INDEX_BUFFER);
    indexBuffer_.SetData(d3d11Context, data.IndexBuffer.data());
    vertexBuffer_.Create(d3d11Device, static_cast<u32>(data.VertexBuffer.size_bytes()), D3D11_BIND_VERTEX_BUFFER);
    vertexBuffer_.SetData(d3d11Context, data.VertexBuffer.data());
}

void Mesh::Draw(ComPtr<ID3D11DeviceContext> d3d11Context)
{
    //Bind index and vertex buffers
    u32 vertexOffset = 0;
    d3d11Context->IASetIndexBuffer(indexBuffer_.Get(), indexBufferFormat_, 0);
    d3d11Context->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &vertexStride_, &vertexOffset);

    if (numLods_ == 1) //In this the mesh is split into several submeshes
    {
        for (auto& submesh : info_.Submeshes)
        {
            u32 firstBlock = submesh.RenderBlocksOffset;
            for (u32 i = 0; i < submesh.NumRenderBlocks; i++)
            {
                RenderBlock& block = info_.RenderBlocks[firstBlock + i];
                d3d11Context->DrawIndexed(block.NumIndices, block.StartIndex, 0);
            }
        }
    }
    else //In this case each submesh is a different lod level
    {
        auto& submesh = info_.Submeshes[lodLevel_];
        u32 firstBlock = submesh.RenderBlocksOffset;
        for (u32 i = 0; i < submesh.NumRenderBlocks; i++)
        {
            RenderBlock& block = info_.RenderBlocks[firstBlock + i];
            d3d11Context->DrawIndexed(block.NumIndices, block.StartIndex, 0);
        }
    }
}

void Mesh::SetLodLevel(u32 level)
{
    if (level < numLods_)
        lodLevel_ = level;
}

u32 Mesh::GetFormatStride(DXGI_FORMAT format)
{
    //Todo: Add all other formats. So far only added ones that are in use
    switch (format)
    {
    case DXGI_FORMAT_R16_UINT:
        return 2;
    default:
        THROW_EXCEPTION("Invalid or unsupported format \"{}\" passed to Mesh::GetFormatStride()", (u32)format);
    }
}
