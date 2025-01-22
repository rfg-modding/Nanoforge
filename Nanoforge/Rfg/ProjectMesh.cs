using System.Collections.Generic;
using Nanoforge.Editor;
using RFGM.Formats.Asset;
using RFGM.Formats.Meshes.Shared;

namespace Nanoforge.Rfg;

public class ProjectMesh : EditorObject
{
    public uint NumVertices;
    public byte VertexStride;
    public VertexFormat VertexFormat;
    public uint NumIndices;
    public byte IndexSize;
    public PrimitiveTopology Topology;
    public List<SubmeshData> Submeshes = new();
    public List<RenderBlock> RenderBlocks = new();

    public ProjectBuffer? IndexBuffer;
    public ProjectBuffer? VertexBuffer;

    public void InitFromRfgMeshConfig(MeshConfig config)
    {
        NumVertices = config.NumVertices;
        VertexStride = config.VertexStride0;
        VertexFormat = config.VertexFormat;
        NumIndices = config.NumIndices;
        IndexSize = config.IndexSize;
        Topology = config.Topology;
        Submeshes.AddRange(config.Submeshes);
        RenderBlocks.AddRange(config.RenderBlocks);
    }
}