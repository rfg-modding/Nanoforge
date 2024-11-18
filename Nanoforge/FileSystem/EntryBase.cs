namespace Nanoforge.FileSystem;

//Base class used by FileSystem entries in PackfileVFS
public class EntryBase(string name)
{
    public EntryBase? Parent = null;
    public readonly string Name = name;
    public long DataBlockOffset { get; private set; } = 0;
    public long DataOffset { get; private set; } = 0;
    public uint Size { get; private set; } = 0;
    public uint CompressedSize { get; private set; } = 0;

    public virtual bool IsDirectory => false;
    public virtual bool IsFile => false;
}