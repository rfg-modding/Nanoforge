using System;
using System.Collections.Generic;
using System.Numerics;
using System.Runtime.CompilerServices;
using Nanoforge.Render.Materials;
using Nanoforge.Render.Misc;
using Serilog;
using Silk.NET.OpenAL;
using Silk.NET.Vulkan;

namespace Nanoforge.Render.Resources;

public abstract class RenderObjectBase(Vector3 position, Matrix4x4 orient, Vector3 scale)
{
    public Vector3 Position = position;
    public Matrix4x4 Orient = orient;
    public Vector3 Scale = scale;

    public virtual unsafe void WriteDrawCommands(List<RenderCommand> commands, Camera camera, GpuFrameDataWriter constants)
    {

    }

    public virtual void Destroy()
    {

    }
}