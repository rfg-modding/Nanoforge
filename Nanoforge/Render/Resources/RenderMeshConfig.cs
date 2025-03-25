using System.Collections.Generic;
using RFGM.Formats.Meshes.Shared;
using Silk.NET.Vulkan;

namespace Nanoforge.Render.Resources;

//Data required to make a renderer mesh. Assumed to be using the unified renderer vertex & index formats.
public class RenderMeshData
{
    public List<SubmeshData> Submeshes;
    public List<RenderBlock> RenderBlocks;
    public IndexType IndexType;
    public byte[] Vertices;
    public byte[] Indices;
    public uint NumVertices;
    public uint NumIndices;

    public RenderMeshData(List<SubmeshData> submeshes, List<RenderBlock> renderBlocks, IndexType indexType, byte[] vertices, byte[] indices, uint numVertices, uint numIndices)
    {
        Submeshes = submeshes;
        RenderBlocks = renderBlocks;
        IndexType = indexType;
        Vertices = vertices;
        Indices = indices;
        NumVertices = numVertices;
        NumIndices = numIndices;
    }
}