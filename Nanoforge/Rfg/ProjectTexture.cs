
using System;
using System.Text.Json.Serialization;
using Nanoforge.Editor;

namespace Nanoforge.Rfg;

public class ProjectTexture : EditorObject
{
    public ProjectBuffer? Data;
    //TODO: Change this to the equivalent for whichever graphics API ends up getting used in the renderer
    public uint Format;
    public uint Width;
    public uint Height;
    public uint NumMipLevels;

    [JsonConstructor]
    private ProjectTexture()
    {
        Data = null;
    }
    
    public ProjectTexture(ProjectBuffer data)
    {
        Data = data;
    }

    public ProjectTexture(byte[] data, string bufferName = "")
    {
        Data = NanoDB.CreateBuffer(data, bufferName) ?? throw new Exception($"Failed to create ProjectBuffer '{bufferName}' for ProjectTexture");
    }
    
    //TODO: Port the helper functions on this class. Its only been partly ported for now to use in NanoDB tests for an EditorObject that references a ProjectBuffer
}

