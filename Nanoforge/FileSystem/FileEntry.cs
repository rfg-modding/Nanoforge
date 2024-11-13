namespace Nanoforge.FileSystem;

//Primitive file - anything that isn't a vpp_pc or str2_pc
public class FileEntry(string name) : EntryBase(name)
{
    public override bool IsDirectory => false;
    public override bool IsFile => true;
    private byte[]? _data = null;
    public bool Preloaded => _data != null;
}