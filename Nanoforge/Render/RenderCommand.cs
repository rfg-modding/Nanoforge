using Nanoforge.Render.Resources;

namespace Nanoforge.Render;

public struct RenderCommand
{
    public Mesh Mesh;
    public uint StartIndex;
    public uint IndexCount;
    public uint ObjectIndex;
}