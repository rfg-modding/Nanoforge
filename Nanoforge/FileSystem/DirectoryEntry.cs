using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using ICSharpCode.SharpZipLib.Zip.Compression.Streams;
using RFGM.Formats.Streams;
using Serilog;

namespace Nanoforge.FileSystem;

//Packfile or container (vpp_pc/str2_pc)
public class DirectoryEntry(string name, bool compressed = false, bool condensed = false, long dataBlockSize = 0, long dataBlockSizeCompressed = 0, long dataBlockOffset = 0, long dataOffset = 0, uint size = 0, uint compressedSize = 0)
    : EntryBase(name, dataBlockOffset, dataOffset, size, compressedSize), IEnumerable<EntryBase>
{
    public List<EntryBase> Entries = new();
    public override bool IsDirectory => true;
    public override bool IsFile => false;
    public bool Compressed => compressed;
    public bool Condensed => condensed;
    public bool Compacted => compressed && condensed;
    public long DataBlockSize => dataBlockSize;
    public long DataBlockSizeCompressed => dataBlockSizeCompressed;
    
    public override Stream? OpenStream()
    {
        try
        {
            bool inAnotherPackfile = Parent != null && Parent != PackfileVFS.Root;
            if (inAnotherPackfile)
            {
                DirectoryEntry parentEntry = (DirectoryEntry)Parent!;
                if (parentEntry.Compacted)
                {
                    //Don't bother handling this case. The game doesn't compact any vpp_pc files, and it's a hassle to deal with since you need to decompress the whole data block up until the data you actually want.
                    Log.Error($"Tried reading {Name} from {Parent!.Name}, which is compacted (compressed AND condensed). PackfileVFS doesn't support reading from vpps packed that way. Please repack the vpp with a different set of flags.");
                    return null;
                }

                if (parentEntry.Compressed)
                {
                    using Stream? parentStream = parentEntry.OpenStream();
                    if (parentStream == null)
                    {
                        Log.Error($"DirectoryEntry.OpenStream() failed to open parent stream for {Name}");
                        return null;
                    }

                    StreamView compressedStream = new(parentStream, parentEntry.DataBlockOffset + DataOffset, CompressedSize);
                    using InflaterInputStream inflaterStream = new(compressedStream);
                    var inflateView = new StreamView(inflaterStream, 0, Size);
                    var decompressedStream = new MemoryStream();
                    //Note: This ends up loading the entire container into memory. Acceptable for the time being
                    inflateView.CopyTo(decompressedStream);
                    decompressedStream.Seek(0, SeekOrigin.Begin);
                    
                    return decompressedStream;
                }
                else
                {
                    Stream? parentStream = parentEntry.OpenStream();
                    if (parentStream == null)
                    {
                        Log.Error($"DirectoryEntry.OpenStream() failed to open parent stream for {Name}");
                        return null;
                    }

                    OwningStreamView view = new(parentStream, parentEntry.DataBlockOffset + DataOffset, Size);
                    return view;
                }
            }
            else
            {
                string path = $"{PackfileVFS.DirectoryPath}{Name}";
                return new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
            }
        }
        catch (Exception ex)
        {
            Log.Error($"Exception in DirectoryEntry.OpenStream() for {Name}. Ex: {ex.Message}");
            return null;
        }
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
        return new DirectoryEnumerator(Entries.GetEnumerator());
    }

    IEnumerator IEnumerable.GetEnumerator()
    {
        return new DirectoryEnumerator(Entries.GetEnumerator());
    }

    public void Preload(bool recursive)
    {
        try
        {
            Log.Information($"Preloading {Name}{(recursive ? " (recursive)" : "")}...");
            using Stream? stream = OpenStream();
            if (stream is null)
            {
                Log.Error($"DirectoryEntry.Preload() failed to open stream for {Name}");
                return;
            }

            if (Compacted)
            {
                byte[] inflateBuffer = new byte[DataBlockSize];

                //For compacted files we must decompress the entire block
                StreamView compressedView = new(stream, DataBlockOffset, DataBlockSizeCompressed);
                using InflaterInputStream inflaterStream = new(compressedView);
                int bytesRead = inflaterStream.Read(inflateBuffer);
                if (bytesRead != inflateBuffer.Length)
                {
                    Log.Error($"In DirectoryEntry.Preload(). Failed to inflate data for file entry '{Name}'. Expected {inflateBuffer.Length} bytes but got {bytesRead}.");
                    return;
                }

                foreach (var entry in Entries)
                {
                    if (entry is FileEntry fileEntry)
                    {
                        byte[] entryData = new byte[fileEntry.Size];
                        Array.Copy(sourceArray: inflateBuffer, sourceIndex: entry.DataOffset,
                                   destinationArray: entryData, destinationIndex: 0, length: entryData.Length);
                        
                        fileEntry.Data = entryData;
                    }
                }
            }
            else if (Compressed)
            {

                foreach (var entry in Entries)
                {
                    if (entry is FileEntry fileEntry)
                    {
                        byte[] inflateBuffer = new byte[fileEntry.Size];

                        stream.Seek(0, SeekOrigin.Begin);
                        StreamView view = new(stream, 0, stream.Length);
                        using InflaterInputStream inflaterStream = new(view); //Give the inflater stream a view so it doesn't close the main stream before we're done with it
                        inflaterStream.Skip(DataBlockOffset + fileEntry.DataOffset); //InflaterInputStream cannot seek
                
                        var bytesRead = inflaterStream.Read(inflateBuffer);
                        if (bytesRead != inflateBuffer.Length)
                        {
                            Log.Error($"In DirectoryEntry.Preload(). Failed to inflate data for file entry '{Name}'. Expected {inflateBuffer.Length} bytes but got {bytesRead}.");
                            return;
                        }

                        fileEntry.Data = inflateBuffer;
                    }
                }
            }
            else
            {
                foreach (var entry in Entries)
                {
                    if (entry is FileEntry fileEntry)
                    {
                        byte[] bytes = new byte[fileEntry.Size];
                        stream.Seek(DataBlockOffset + fileEntry.DataOffset, SeekOrigin.Begin);
                        int bytesRead = stream.Read(bytes);
                        if (bytesRead != bytes.Length)
                        {
                            Log.Error($"In DirectoryEntry.Preload(). Failed to read data for file entry '{fileEntry.Name}'. Expected {bytes.Length} bytes but got {bytesRead}.");
                            return;
                        }

                        fileEntry.Data = bytes;
                    }
                }
            }

            if (recursive)
            {
                foreach (EntryBase entry in Entries)
                {
                    if (entry is DirectoryEntry directoryEntry)
                    {
                        directoryEntry.Preload(true);
                    }
                }
            }
        }
        catch (Exception ex)
        {
            Log.Error($"Exception in DirectoryEntry.Preload() for {Name}. Ex: {ex.Message}");
            throw;
        }
    }

    public void Unload(bool recursive)
    {
        try
        {
            Log.Information($"Unloading cached files for {Name}{(recursive ? " (recursive)" : "")}...");
            foreach (EntryBase entry in Entries)
            {
                if (entry is FileEntry fileEntry)
                {
                    fileEntry.Data = null;
                }
                else if (recursive && entry is DirectoryEntry directoryEntry)
                {
                    directoryEntry.Unload(true);
                }
            }
            
            //Trigger garbage collection to clean up any of the arrays that have no references left.
            //This lowers memory usage by a decent amount, but it doesn't go back to the level it was before the preload.
            //I believe that's just the allocator holding onto memory so future allocations are quicker (doesn't need to ask the OS for memory again).
            GC.Collect();
        }
        catch (Exception ex)
        {
            Log.Error($"Exception in DirectoryEntry.Unload() for {Name}. Ex: {ex.Message}");
            throw;
        }
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
        throw new NotImplementedException();
    }
    
    public void Dispose()
    {
        _enumerator.Dispose();
    }
}

