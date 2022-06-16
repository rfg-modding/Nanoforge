#pragma once
#include "common/Typedefs.h"
#include "render/resources/Buffer.h"
#include <RfgTools++/formats/meshes/MeshHelpers.h>
#include <filesystem>
#include <d3d11.h>
#include <memory>
#include <wrl.h>
#include <span>
using Microsoft::WRL::ComPtr;

//TODO: Come up with a better way of doing this. Used by RenderChunk to override which submeshes to render. Took the simplest approach first to get it working. Long term this should be replaced.
extern u16 SubmeshOverrideIndices[256];

//Represents a 3d mesh. Made up of a vertex buffer and an index buffer
class Mesh
{
public:
    Mesh() {}
    Mesh(ComPtr<ID3D11Device> d3d11Device, const MeshInstanceData& data, u32 numLods = 1);
    //Bind vertex and index buffers to context
    void Draw(ComPtr<ID3D11DeviceContext> d3d11Context, u32 numSubmeshOverrides = -1);
    //Get underlying pointer to d3d11 vertex buffer
    ID3D11Buffer* GetVertexBuffer() { return vertexBuffer_.Get(); }
    //Get underlying pointer to d3d11 index buffer
    ID3D11Buffer* GetIndexBuffer() { return indexBuffer_.Get(); }
    //Get num vertices
    u32 NumVertices() { return numVertices_; }
    //Get num indices
    u32 NumIndices() { return numIndices_; }
    u32 NumSubmeshes() { return info_->NumSubmeshes; }

    u32 NumLods() { return numLods_; }
    u32 GetLodLevel() { return lodLevel_; }
    void SetLodLevel(u32 level);

    //Todo: Move into util namespace
    //Returns the size in bytes of an element of the provided format
    static u32 GetFormatStride(DXGI_FORMAT format);

    const MeshDataBlock& GetInfo() const { return *info_.get(); }

private:
    u32 numLods_ = 0;
    u32 lodLevel_ = 0;
    //Note: While this struct is relatively small, it can cause huge memory usage when the Mesh is duplicated many times. 
    //      Specifically for chunks. Since the mesh gets duplicated for each subpiece. Since the buffers are shallow copied every mesh instance still points to the same GPU data, meaning those don't get duped.
    //      I thought it was fine to duplicate the MeshDataBlock since it's fairly small. But for things like chunks, which have many Submeshes and RenderBlocks, it can take up several GB of space.
    std::shared_ptr<MeshDataBlock> info_ = nullptr;
    Buffer vertexBuffer_;
    Buffer indexBuffer_;

    u32 numVertices_ = 0;
    u32 numIndices_ = 0;
    u32 vertexStride_ = 0;
    u32 indexStride_ = 0;
    DXGI_FORMAT indexBufferFormat_ = DXGI_FORMAT_UNKNOWN;
    D3D11_PRIMITIVE_TOPOLOGY topology_ = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
};