using System.Collections;
using System.Collections.Generic;
using RFGM.Formats.Vpp.Models;

namespace Nanoforge.FileSystem;

//Packfile or container (vpp_pc/str2_pc)
public class DirectoryEntry(string name, LogicalArchive? archive = null) : EntryBase(name), IEnumerable<EntryBase>
{
    public List<EntryBase> Entries = new();
    public override bool IsDirectory => true;
    public override bool IsFile => false;
    public bool Compressed => Archive != null && Archive.Mode.HasFlag(RfgVpp.HeaderBlock.Mode.Compressed);
    public bool Condensed => Archive != null && Archive.Mode.HasFlag(RfgVpp.HeaderBlock.Mode.Condensed);

    public LogicalArchive? Archive { get; set; } = archive;

    //Write @streams.xml file that lists all the files in a packfile. Used during packfile repacking to ensure that file order is preserved on str2_pc files
    //The order must be preserved or the str2_pc file will be out of sync with the asm_pc files and break the game.
    public void WriteStreamsFile(string outputFolderPath)
    {
        //TODO: PORT
        // Xml xml = scope .();
        // XmlNode streams = xml.AddChild("streams");
        // streams.AttributeList.Add("endian", "Little");
        // streams.AttributeList.Add("compressed", Compressed ? "True" : "False");
        // streams.AttributeList.Add("condensed", Condensed ? "True" : "False");
        //
        // for (EntryBase entry in this.Entries)
        // {
        //     XmlNode xmlNode = streams.AddChild("entry");
        //     xmlNode.AttributeList.Add("name", .. scope String(entry.Name));
        //     xmlNode.NodeValue.Set(entry.Name);
        // }
        // xml.SaveToFile(scope $@"{outputFolderPath}\@streams.xml");
    }

    public IEnumerator<EntryBase> GetEnumerator()
    {
        return new DirectoryEnumerator(Entries.GetEnumerator());
    }

    IEnumerator IEnumerable.GetEnumerator()
    {
        return new DirectoryEnumerator(Entries.GetEnumerator());
    }
}

public class DirectoryEnumerator : IEnumerator<EntryBase>
{
    private List<EntryBase>.Enumerator _enumerator;
    public EntryBase Current { get; private set; }
    object? IEnumerator.Current => _enumerator.Current;
    
    public DirectoryEnumerator(List<EntryBase>.Enumerator enumerator)
    {
        _enumerator = enumerator;
        Current = _enumerator.Current;
    }

    public bool MoveNext()
    {
        bool result = _enumerator.MoveNext();
        Current = _enumerator.Current;
        return result;
    }

    public void Reset()
    {
        throw new System.NotImplementedException();
    }
    
    public void Dispose()
    {
        _enumerator.Dispose();
    }
}

