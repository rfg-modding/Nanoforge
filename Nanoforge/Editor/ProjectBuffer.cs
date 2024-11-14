using System;
using System.IO;
using System.Threading;

namespace Nanoforge.Editor;

//Binary blob of data attached to the project. Useful for bulk binary data such as textures and meshes.
public class ProjectBuffer
{
    // ReSharper disable once InconsistentNaming
    public const ulong NullUID = ulong.MaxValue;
    
    // ReSharper disable once InconsistentNaming
    public ulong UID = NullUID;
    public int Size { get; private set; } = 0;
    public string Name = "";
    private object _lock = new Mutex();

    public ProjectBuffer(ulong uid, int size = 0, string name = "")
    {
        UID = uid;
        Size = size;
        Name = name;
    }

    public string GetPath()
    {
        return $"{NanoDB.BuffersDirectory}{Name}.{UID}.buffer";
    }

    public byte[] Load()
    {
        lock (_lock)
        {
            byte[] bytes = File.ReadAllBytes(GetPath());
            return bytes;
        }
    }
    
    public bool Save(Span<byte> data)
    {
        //TODO: Port tiny buffer merge optimization from the C++ version. Prevents having 1000s of 1KB or less files in the buffers folder since that causes performance issues.
        lock (_lock)
        {
            if (!Directory.Exists(NanoDB.BuffersDirectory))
            {
                Directory.CreateDirectory(NanoDB.BuffersDirectory);
            }
        
            File.WriteAllBytes(GetPath(), data.ToArray());
            return true;
        }
    }
}