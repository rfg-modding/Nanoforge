using System.Collections.Generic;
using System.Numerics;
using Nanoforge.Editor;

namespace Nanoforge.Rfg;

public class Territory : EditorObject
{
    public Vector3 Position = Vector3.Zero;
    public string PackfileName = string.Empty;
    public bool Compressed = false;
    public bool Condensed = false;
    public bool Compacted => Compressed && Condensed;

    public List<Zone> Zones = new();

    //TODO: Port
}

public class Chunk : EditorObject
{
    //TODO: Port
}