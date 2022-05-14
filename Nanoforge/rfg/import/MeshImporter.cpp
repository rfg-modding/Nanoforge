#include "Importers.h"
#include <RfgTools++/formats/meshes/MeshDataBlock.h>

ObjectHandle Importers::ImportMeshHeader(MeshDataBlock& mesh)
{
    Registry& registry = Registry::Get();
    ObjectHandle meshObj = registry.CreateObject("", "Mesh");
    meshObj.Property("Version").Set<u32>(mesh.Version);
    meshObj.Property("VerificationHash").Set<u32>(mesh.VerificationHash);
    meshObj.Property("CpuDataSize").Set<u32>(mesh.CpuDataSize);
    meshObj.Property("GpuDataSize").Set<u32>(mesh.GpuDataSize);
    meshObj.Property("NumSubmeshes").Set<u32>(mesh.NumSubmeshes);
    meshObj.Property("SubmeshesOffset").Set<u32>(mesh.SubmeshesOffset);
    meshObj.Property("NumVertices").Set<u32>(mesh.NumVertices);
    meshObj.Property("VertexStride0").Set<u8>(mesh.VertexStride0);
    meshObj.Property("VertexFormat").Set<u8>((u8)mesh.VertFormat);
    meshObj.Property("NumUvChannels").Set<u8>(mesh.NumUvChannels);
    meshObj.Property("VertexStride1").Set<u8>(mesh.VertexStride1);
    meshObj.Property("VertexOffset").Set<u32>(mesh.VertexOffset);
    meshObj.Property("NumIndices").Set<u32>(mesh.NumIndices);
    meshObj.Property("IndicesOffset").Set<u32>(mesh.IndicesOffset);
    meshObj.Property("IndexSize").Set<u8>(mesh.IndexSize);
    meshObj.Property("PrimitiveType").Set<u8>((u8)mesh.PrimitiveType);
    meshObj.Property("NumRenderBlocks").Set<u16>(mesh.NumRenderBlocks);

    for (SubmeshData& submeshData : mesh.Submeshes)
    {
        ObjectHandle submesh = registry.CreateObject("", "Submesh");
        submesh.Property("NumRenderBlocks").Set<u32>(submeshData.NumRenderBlocks);
        submesh.Property("Offset").Set<Vec3>(submeshData.Offset);
        submesh.Property("Bmin").Set<Vec3>(submeshData.Bmin);
        submesh.Property("Bmax").Set<Vec3>(submeshData.Bmax);
        submesh.Property("RenderBlocksOffset").Set<u32>(submeshData.RenderBlocksOffset);

        meshObj.Property("Submeshes").GetObjectList().push_back(submesh);
    }

    for (RenderBlock& renderBlock : mesh.RenderBlocks)
    {
        ObjectHandle block = registry.CreateObject("", "RenderBlock");
        block.Property("MaterialMapIndex").Set<u16>(renderBlock.MaterialMapIndex);
        block.Property("StartIndex").Set<u32>(renderBlock.StartIndex);
        block.Property("NumIndices").Set<u32>(renderBlock.NumIndices);
        block.Property("MinIndex").Set<u32>(renderBlock.MinIndex);
        block.Property("MaxIndex").Set<u32>(renderBlock.MaxIndex);

        meshObj.Property("RenderBlocks").GetObjectList().push_back(block);
    }

    return meshObj;
}