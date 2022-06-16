#include "Mesh.h"
#include "Log.h"

u16 SubmeshOverrideIndices[256] = { 0 };

Mesh::Mesh(ComPtr<ID3D11Device> d3d11Device, const MeshInstanceData& data, u32 numLods)
{
    //All RFG meshes seen so far use these
    static const D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

    //Test the current assumption that each lod level has only 1 submesh when multiple lods exist
    if (numLods != 1)
        assert(numLods == data.Info.NumSubmeshes);

    //Data layout
    vertexStride_ = data.Info.VertexStride0;
    indexStride_ = data.Info.IndexSize;
    numVertices_ = data.Info.NumVertices;
    numIndices_ = data.Info.NumIndices;
    if (data.Info.IndexSize == 2)
        indexBufferFormat_ = DXGI_FORMAT_R16_UINT;
    else if (data.Info.IndexSize == 4)
        indexBufferFormat_ = DXGI_FORMAT_R32_UINT;
    else
        THROW_EXCEPTION("Unsupported mesh index stride of {}", data.Info.IndexSize);

    topology_ = Topology;
    numLods_ = numLods;
    info_ = std::make_shared<MeshDataBlock>(data.Info);

    //Create and set data for index and vertex buffers
    indexBuffer_.Create(d3d11Device, static_cast<u32>(data.IndexBuffer.size()), D3D11_BIND_INDEX_BUFFER, (void*)data.IndexBuffer.data());
    vertexBuffer_.Create(d3d11Device, static_cast<u32>(data.VertexBuffer.size()), D3D11_BIND_VERTEX_BUFFER, (void*)data.VertexBuffer.data());
}

void Mesh::Draw(ComPtr<ID3D11DeviceContext> d3d11Context, u32 numSubmeshOverrides)
{
    //Bind index and vertex buffers
    u32 vertexOffset = 0;
    d3d11Context->IASetIndexBuffer(indexBuffer_.Get(), indexBufferFormat_, 0);
    d3d11Context->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &vertexStride_, &vertexOffset);

    if (numSubmeshOverrides != -1)
    {
        for (size_t i = 0; i < numSubmeshOverrides; i++)
        {
            u32 submeshIndex = SubmeshOverrideIndices[i];
            auto& submesh = info_->Submeshes[submeshIndex];
            u32 firstBlock = submesh.RenderBlocksOffset;
            for (u32 i = 0; i < submesh.NumRenderBlocks; i++)
            {
                RenderBlock& block = info_->RenderBlocks[firstBlock + i];
                d3d11Context->DrawIndexed(block.NumIndices, block.StartIndex, 0);
            }
        }

    }
    else if (numLods_ == 1) //In this the mesh is split into several submeshes
    {
        for (auto& submesh : info_->Submeshes)
        {
            u32 firstBlock = submesh.RenderBlocksOffset;
            for (u32 i = 0; i < submesh.NumRenderBlocks; i++)
            {
                RenderBlock& block = info_->RenderBlocks[firstBlock + i];
                d3d11Context->DrawIndexed(block.NumIndices, block.StartIndex, 0);
            }
        }
    }
    else //In this case each submesh is a different lod level
    {
        auto& submesh = info_->Submeshes[lodLevel_];
        u32 firstBlock = submesh.RenderBlocksOffset;
        for (u32 i = 0; i < submesh.NumRenderBlocks; i++)
        {
            RenderBlock& block = info_->RenderBlocks[firstBlock + i];
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
