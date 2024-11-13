using System.Collections;
using System.Collections.Generic;

namespace Nanoforge.FileSystem;

//Packfile or container (vpp_pc/str2_pc)
public class DirectoryEntry : EntryBase, IEnumerable<EntryBase>
{
    public List<EntryBase> Entries = new();
    public override bool IsDirectory => true;
    public override bool IsFile => false;
    public bool Compressed { get; private set; } = false;
    public bool Condensed  { get; private set; } = false;

    public DirectoryEntry(string name, bool compressed = false, bool condensed = false) : base(name)
    {
        Compressed = compressed;
        Condensed = condensed;
    }
    
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
        //TODO: IMPLEMENT
        throw new System.NotImplementedException();
    }

    IEnumerator IEnumerable.GetEnumerator()
    {
        //TODO: IMPLEMENT
        return GetEnumerator();
    }
}