using System.Collections.Generic;
using System.Numerics;
using Nanoforge.Editor;
using Nanoforge.Render;

namespace Nanoforge.Rfg;

public class Territory : EditorObject
{
    public Vector3 Position = Vector3.Zero;
    public string PackfileName = string.Empty;
    public bool Compressed = false;
    public bool Condensed = false;
    public bool Compacted => Compressed && Condensed;

    public List<Zone> Zones = new();

    public bool Load(Renderer renderer, Scene scene)
    {
        //TODO: Implement loading map data such as meshes, textures, and objects from project files
        return true;
    }
    
    //TODO: Port the rest of this classes functions and fields
}

public class Chunk : EditorObject
{
    //TODO: Port
}