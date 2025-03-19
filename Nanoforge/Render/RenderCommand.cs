using Nanoforge.Render.Materials;
using Nanoforge.Render.Misc;
using Nanoforge.Render.Resources;

namespace Nanoforge.Render;

public struct RenderCommand
{
    public Mesh Mesh;
    public MaterialPipeline Pipeline;
    public uint StartIndex;
    public uint IndexCount;
    public uint ObjectIndex;
}