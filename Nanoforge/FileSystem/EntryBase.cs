using System;
using System.IO;

namespace Nanoforge.FileSystem;

//Base class used by FileSystem entries in PackfileVFS
public class EntryBase(string name, long dataBlockOffset = long.MaxValue, long dataOffset = long.MaxValue, uint size = uint.MaxValue, uint compressedSize = uint.MaxValue)
{
    public EntryBase? Parent = null;
    public readonly string Name = name;
    public long DataBlockOffset { get; private set; } = dataBlockOffset;
    public long DataOffset { get; private set; } = dataOffset;
    public uint Size { get; private set; } = size;
    public uint CompressedSize { get; private set; } = compressedSize;

    public virtual bool IsDirectory => false;
    public virtual bool IsFile => false;

    public virtual Stream? OpenStream()
    {
        throw new NotImplementedException();
    }

    public virtual byte[]? ReadAllBytes()
    {
        throw new NotImplementedException();
    }

    public virtual string? ReadAllText()
    {
        throw new NotImplementedException();
    }
}