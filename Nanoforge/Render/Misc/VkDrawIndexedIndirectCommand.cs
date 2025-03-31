using System.Runtime.InteropServices;

namespace Nanoforge.Render.Misc;

[StructLayout(LayoutKind.Sequential)]
public struct VkDrawIndexedIndirectCommand
{
    public uint IndexCount;
    public uint InstanceCount;
    public uint FirstIndex;
    public int VertexOffset;
    public uint FirstInstance;

    public VkDrawIndexedIndirectCommand(uint indexCount, uint instanceCount, uint firstIndex, int vertexOffset, uint firstInstance)
    {
        IndexCount = indexCount;
        InstanceCount = instanceCount;
        FirstIndex = firstIndex;
        VertexOffset = vertexOffset;
        FirstInstance = firstInstance;
    }
}

// struct VkDrawIndexedIndirectCommand {
//     uint32_t    indexCount;
//     uint32_t    instanceCount;
//     uint32_t    firstIndex;
//     int32_t     vertexOffset;
//     uint32_t    firstInstance;
// };
