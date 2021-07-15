#include "Mesh.h"
#include "Log.h"

void Mesh::Create(ComPtr<ID3D11Device> d3d11Device, ComPtr<ID3D11DeviceContext> d3d11Context, std::span<u8> vertexBytes, std::span<u8> indexBytes,
                  u32 numVertices, DXGI_FORMAT indexBufferFormat, D3D11_PRIMITIVE_TOPOLOGY topology)
{
    //Store args in member variabes for later use
    numVertices_ = numVertices;
    vertexStride_ = vertexBytes.size_bytes() / numVertices;
    indexStride_ = GetFormatStride(indexBufferFormat);
    numIndices_ = indexBytes.size_bytes() / indexStride_;
    indexBufferFormat_ = indexBufferFormat;
    topology_ = topology;

    //Create and set data for index and vertex buffers
    indexBuffer_.Create(d3d11Device, static_cast<u32>(indexBytes.size_bytes()), D3D11_BIND_INDEX_BUFFER);
    indexBuffer_.SetData(d3d11Context, indexBytes.data());
    vertexBuffer_.Create(d3d11Device, static_cast<u32>(vertexBytes.size_bytes()), D3D11_BIND_VERTEX_BUFFER);
    vertexBuffer_.SetData(d3d11Context, vertexBytes.data());
}

void Mesh::Bind(ComPtr<ID3D11DeviceContext> d3d11Context)
{
    u32 vertexOffset = 0;
    d3d11Context->IASetIndexBuffer(indexBuffer_.Get(), indexBufferFormat_, 0);
    d3d11Context->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &vertexStride_, &vertexOffset);
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
