using Nanoforge.Render.Materials;
using Nanoforge.Render.Misc;
using Nanoforge.Render.Resources;

namespace Nanoforge.Render;

public struct RenderCommand
{
    public Mesh Mesh;
    public MaterialInstance MaterialInstance;
    public uint StartIndex;
    public uint IndexCount;
    public uint ObjectIndex;
}