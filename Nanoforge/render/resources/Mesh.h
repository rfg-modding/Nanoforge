#pragma once
#include "common/Typedefs.h"
#include "render/resources/Buffer.h"
#include <RfgTools++/formats/meshes/MeshHelpers.h>
#include <filesystem>
#include <d3d11.h>
#include <span>
#include <wrl.h>
using Microsoft::WRL::ComPtr;

//Represents a 3d mesh. Made up of a vertex buffer and an index buffer
class Mesh
{
public:
    //Creates mesh from provided data
    void Create(ComPtr<ID3D11Device> d3d11Device, ComPtr<ID3D11DeviceContext> d3d11Context, MeshInstanceData data, u32 numLods = 1);
    //Bind vertex and index buffers to context
    void Draw(ComPtr<ID3D11DeviceContext> d3d11Context);
    //Get underlying pointer to d3d11 vertex buffer
    ID3D11Buffer* GetVertexBuffer() { return vertexBuffer_.Get(); }
    //Get underlying pointer to d3d11 index buffer
    ID3D11Buffer* GetIndexBuffer() { return indexBuffer_.Get(); }
    //Get num vertices
    u32 NumVertices() { return numVertices_; }
    //Get num indices
    u32 NumIndices() { return numIndices_; }

    u32 NumLods() { return numLods_; }
    u32 GetLodLevel() { return lodLevel_; }
    void SetLodLevel(u32 level);

    //Todo: Move into util namespace
    //Returns the size in bytes of an element of the provided format
    static u32 GetFormatStride(DXGI_FORMAT format);

private:
    u32 numLods_;
    u32 lodLevel_ = 0;
    MeshDataBlock info_;
    Buffer vertexBuffer_;
    Buffer indexBuffer_;

    u32 numVertices_ = 0;
    u32 numIndices_ = 0;
    u32 vertexStride_ = 0;
    u32 indexStride_ = 0;
    DXGI_FORMAT indexBufferFormat_ = DXGI_FORMAT_UNKNOWN;
    D3D11_PRIMITIVE_TOPOLOGY topology_ = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
};