#include "Mesh.h"
#include "Log.h"

void Mesh::Create(ID3D11Device* d3d11Device, ID3D11DeviceContext* d3d11Context, std::span<u8> vertexBytes, std::span<u8> indexBytes, 
                  u32 numVertices, DXGI_FORMAT indexBufferFormat, D3D11_PRIMITIVE_TOPOLOGY topology)
{
    //Store args in member variabes for later use
    numVertices_ = numVertices;
    vertexStride_ = vertexBytes.size_bytes() / numVertices;
    indexStride_ = GetFormatStride(indexBufferFormat);
    numIndices_ = indexBytes.size_bytes() / indexStride_;
    indexBufferFormat_ = indexBufferFormat;
    topology_ = topology;

    //Clear existing data so this function can be used for recreation with multiple calls
    vertexBuffer_.Reset();
    indexBuffer_.Reset();

    //Todo: Consider having general buffer class to wrap this behavior with
    //Create index buffer and fill with indexBytes data
    D3D11_BUFFER_DESC indexBufferDesc = {};
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = static_cast<u32>(indexBytes.size_bytes());
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = 0;
    indexBufferDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA indexBufferInitData;
    indexBufferInitData.pSysMem = indexBytes.data();
    if (FAILED(d3d11Device->CreateBuffer(&indexBufferDesc, &indexBufferInitData, indexBuffer_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create index buffer in Mesh::Create()");

    d3d11Context->IASetIndexBuffer(indexBuffer_.Get(), indexBufferFormat_, 0);//Todo: See if this can be done at mesh render time


    //Create vertex buffer for instance
    D3D11_BUFFER_DESC vertexBufferDesc = {};
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = static_cast<u32>(vertexBytes.size_bytes());
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA vertexBufferInitData = {};
    vertexBufferInitData.pSysMem = vertexBytes.data();
    if (FAILED(d3d11Device->CreateBuffer(&vertexBufferDesc, &vertexBufferInitData, vertexBuffer_.GetAddressOf())))
        THROW_EXCEPTION("Failed to create vertex buffer in Mesh::Create()");

    //Set vertex buffer
    u32 vertexOffset = 0;
    d3d11Context->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &vertexStride_, &vertexOffset);//Todo: See if this can be done at mesh render time
    d3d11Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP); //Todo: See if this can be done at mesh render time
}

void Mesh::Bind(ID3D11DeviceContext* d3d11Context)
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
        THROW_EXCEPTION("Invalid format \"{}\" passed to Mesh::GetFormatStride()", (u32)format);
    }
}
