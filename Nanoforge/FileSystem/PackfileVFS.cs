using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using Kaitai;
using Nanoforge.Rfg;
using RFGM.Formats.Streams;
using RFGM.Formats.Vpp;
using RFGM.Formats.Vpp.Models;
using Serilog;

namespace Nanoforge.FileSystem;

//Lets you interact with RFG packfiles (.vpp_pc & .str2_pc version 3) like they're a normal filesystem. Tracks all the vpps in the mounted data folder.
//NOT thread safe. You better make some improvements to this before you try to use it on multiple threads simultaneously.
public static class PackfileVFS
{
    public static DirectoryEntry? Root = null;
    public static string DirectoryPath { get; private set; } = "";
    public static string Mount { get; private set; } = "";
    public static bool Loading { get; private set; } = false;
    public static bool Ready => Root != null && !Loading;

    public static void MountDataFolderInBackground(string mount, string directoryPath)
    {
        ThreadPool.QueueUserWorkItem(_ => MountDataFolder(mount, directoryPath));
    }
    
    private static void MountDataFolder(string mount, string directoryPath)
    {
        try
        {
            Loading = true;
            Mount = mount;
            DirectoryPath = directoryPath;
            if (!Path.EndsInDirectorySeparator(DirectoryPath))
            {
                DirectoryPath += Path.DirectorySeparatorChar;
            }
            Root = new DirectoryEntry(mount);


            int errorCount = 0;
            string[] packfilePaths = Directory.GetFiles(directoryPath, "*.vpp_pc", SearchOption.TopDirectoryOnly);
            foreach (string packfilePath in packfilePaths)
            {
                string packfileName = Path.GetFileName(packfilePath);
                if (!MountPackfile(packfilePath))
                {
                    Log.Error("PackfileVFS failed to mount {packfilePath} in the VFS.");
                    errorCount++;
                    continue;
                }
                
            }

            Root.Entries.Sort((a, b) => String.Compare(a.Name, b.Name, StringComparison.Ordinal));
            foreach (EntryBase entry in Root)
            {
                if (entry is DirectoryEntry directoryEntry)
                {
                    directoryEntry.Entries.Sort((a, b) => String.Compare(a.Name, b.Name, StringComparison.Ordinal));
                }
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error while mounting data folder");
            throw;
        }
        finally
        {
            Loading = false;
        }
    }
    
    private static bool MountPackfile(string path, bool loadIntoMemory = false)
    {
        string packfileName = Path.GetFileName(path);
        using var packfileStream = new KaitaiStream(path);
        RfgVpp packfile = new RfgVpp(packfileStream);
        long[] packfileEntryOffsets = GetPatchedEntryOffsets(packfile);

        DirectoryEntry vfsPackfile = new(
            name: packfileName,
            compressed: packfile.Header.Flags.Mode.HasFlag(RfgVpp.HeaderBlock.Mode.Compressed),
            condensed: packfile.Header.Flags.Mode.HasFlag(RfgVpp.HeaderBlock.Mode.Condensed),
            dataBlockSize: packfile.Header.LenData,
            dataBlockSizeCompressed: packfile.Header.LenCompressedData,
            dataBlockOffset: packfile.BlockOffset,
            dataOffset: 0,
            size: (uint)packfileStream.Size,
            compressedSize: 0)
        {
            Parent = Root,
        };
        Root!.Entries.Add(vfsPackfile);

        for (int i = 0; i < packfile.Entries.Count; i++)
        {
            RfgVpp.Entry? entry = packfile.Entries[i];
            string? entryName = packfile.EntryNames.Values[i].Value;
            if (entry == null || entryName == null)
            {
                Log.Error($"Failed to index entry {i} from {packfileName}");
                throw new Exception($"Failed to index entry {i} from {packfileName}");
            }

            string extension = Path.GetExtension(entryName);
            if (extension == ".str2_pc") //Container. Create a directory entry with the name of the str2 and add its subfiles as FileEntry's
            {
                if (vfsPackfile.Compressed)
                {
                    //Packfile is compressed, so we need to extract the whole str2 (much slower). Not supported since these don't exist in the vanilla game.
                    Log.Error($"{packfileName} is compressed and contains str2_pc files. Can't parse the str2_pc. Please repack that vpp_pc without compression and restart Nanoforge or it won't work properly.");
                    return false;
                }

                //Packfile isn't compressed. Instead of extracting the whole str2 just open a filestream in the vpp and seek to the str2 header
                using FileStream containerFileStream = new(path, FileMode.Open, FileAccess.Read, FileShare.Read);
                StreamView containerStreamView = new(containerFileStream, packfile.BlockOffset + packfileEntryOffsets[i], entry.LenData);
                using KaitaiStream containerKaitaiStream = new(containerStreamView);
                RfgVpp container = new(containerKaitaiStream);

                long[] containerEntryOffsets = GetPatchedEntryOffsets(container);
                DirectoryEntry vfsContainer = new(
                    name: entryName,
                    compressed: container.Header.Flags.Mode.HasFlag(RfgVpp.HeaderBlock.Mode.Compressed),
                    condensed: container.Header.Flags.Mode.HasFlag(RfgVpp.HeaderBlock.Mode.Condensed),
                    dataBlockSize: container.Header.LenData,
                    dataBlockSizeCompressed: container.Header.LenCompressedData,
                    dataBlockOffset: container.BlockOffset,
                    dataOffset: packfileEntryOffsets[i],
                    size: entry.LenData,
                    compressedSize: entry.LenCompressedData)
                {
                    Parent = vfsPackfile,
                };

                for (int j = 0; j < container.Entries.Count; j++)
                {
                    RfgVpp.Entry? containerEntry = container.Entries[j];
                    string? containerEntryName = container.EntryNames.Values[j].Value;
                    if (containerEntry == null || containerEntryName == null)
                    {
                        Log.Error($"Failed to index entry {j} from container {entryName}");
                        throw new Exception($"Failed to index entry {j} from container {entryName}");
                    }

                    FileEntry vfsFile = new(containerEntryName, dataOffset: containerEntryOffsets[j], size: containerEntry.LenData,
                        compressedSize: containerEntry.LenCompressedData)
                    {
                        Parent = vfsContainer
                    };
                    vfsContainer.Entries.Add(vfsFile);
                }

                vfsPackfile.Entries.Add(vfsContainer);
            }
            else //Plain file. Make a FileEntry for it
            {
                FileEntry vfsFile = new(entryName, dataOffset: packfileEntryOffsets[i], size: entry.LenData, compressedSize: entry.LenCompressedData)
                {
                    Parent = vfsPackfile
                };
                vfsPackfile.Entries.Add(vfsFile);
            }
        }

        return true;
    }

    //Note: Based on RfgVpp.FixOffsetOverflow() in RFGM.Formats
    //      The entry offsets need to be patched since they are stored in 32 bit unsigned integers, but RFG has packfiles that with a data block thats larger than that.
    //      This is temporary. Once LogicalArchive/LogicalFile have been upgraded to allow lazy stream creation then PackfileVFS can go back to using those instead of manually using RfgVpp and opening streams.
    private static long[] GetPatchedEntryOffsets(RfgVpp packfile)
    {
        bool compressed = packfile.Header.Flags.Mode.HasFlag(RfgVpp.HeaderBlock.Mode.Compressed);
        bool condensed = packfile.Header.Flags.Mode.HasFlag(RfgVpp.HeaderBlock.Mode.Condensed);
        bool compacted = compressed && condensed;
        if (compacted)
            return packfile.Entries.Select(entry => (long)entry.DataOffset).ToArray(); //If compacted we can ignore the offsets and calculate them on demand since we need to decompress the whole data block anyway & there's no padding bytes
        
        const long alignmentSize = 2048;
        
        long[] entryOffsets = new long[packfile.Entries.Count];
        long runningDataOffset = 0; //Track relative offset from data section start
        for (int i = 0; i < packfile.Entries.Count; i++)
        {
            RfgVpp.Entry entryData = packfile.Entries[i];
            entryOffsets[i] = runningDataOffset;
            
            if (compressed)
            {
                runningDataOffset += entryData.LenCompressedData;
                long alignmentPad = StreamHelpers.CalcAlignment(runningDataOffset, alignmentSize);
                runningDataOffset += alignmentPad;
            }
            else
            {
                runningDataOffset += entryData.LenData;
                if (!condensed)
                {
                    long alignmentPad = StreamHelpers.CalcAlignment(runningDataOffset, alignmentSize);
                    runningDataOffset += alignmentPad;
                }
            }
        }

        return entryOffsets;
    }

    public static EntryBase? GetEntry(string path)
    {
        if (Root == null)
        {
            Log.Error("PackfileVFS.GetEntry() failed. No root directory. Mount the RFG directory first.");
            return null;
        }
        
        //Strip mount point. E.g. //data/
        if (!path.StartsWith(Mount))
        {
            Log.Error($"Invalid path '{path}' passed to VFS. Path must start with the mount point '{Mount}'");
            return null;
        }

        path = path.Remove(0, Mount.Length);
        if (path.Length == 0)
            return null;

        DirectoryEntry directory = Root;
        while (path.Length > 0)
        {
            int nextDelimiter = path.IndexOf("/", StringComparison.Ordinal);
            if (nextDelimiter == -1)
            {
                //No more slashes. We hit the end of the path on a file
                FileEntry? result = (FileEntry?)directory.Entries.Select(entry => entry)
                                                                 .FirstOrDefault(entry => entry.Name.Equals(path, StringComparison.OrdinalIgnoreCase) && entry.IsFile);
                
                return result;
            }
            else if (nextDelimiter == path.Length - 1)
            {
                //Slash on the end of the path. We hit the end of the path on a directory
                path = path.Substring(0, path.Length - 1); //Remove slash from end so we can find a directory with that name
                DirectoryEntry? result = (DirectoryEntry?)directory.Entries.Select(entry => entry)
                                                                           .FirstOrDefault(entry => entry.Name.Equals(path, StringComparison.OrdinalIgnoreCase) && entry.IsDirectory);
                
                return result;                
            }
            else
            {
                //Slash in the middle of the path. Find the next entry and proceed
                string part = path.Substring(0, nextDelimiter);
                path = path.Remove(0, nextDelimiter + 1);
                DirectoryEntry? result = (DirectoryEntry?)directory.Entries.Select(entry => entry)
                                                                           .FirstOrDefault(entry => entry.Name.Equals(part, StringComparison.OrdinalIgnoreCase) && entry.IsDirectory);

                if (result == null)
                    return null;
                
                directory = result;
            }
        }
        
        return null;
    }

    public static IEnumerable<EntryBase> Enumerate(string path)
    {
        EntryBase? entry = GetEntry(path);
        if (entry == null || !entry.IsDirectory)
            return [];
        
        return (entry as DirectoryEntry)!;
    }

    public static bool Exists(string path)
    {
        return GetEntry(path) != null;
    }

    public static Stream? OpenFile(string path)
    {
        return GetEntry(path)?.OpenStream();
    }

    public static byte[]? ReadAllBytes(string path)
    {
        return GetEntry(path)?.ReadAllBytes();
    }

    public static string? ReadAllText(string path)
    {
        return GetEntry(path)?.ReadAllText();
    }

    public static void PreloadDirectory(string path, bool recursive = true)
    {
        EntryBase? entry = GetEntry(path);
        if (entry is DirectoryEntry directoryEntry)
        {
            directoryEntry.Preload(recursive);
        }
    }
    
    public static void UnloadDirectory(string path, bool recursive = true)
    {
        EntryBase? entry = GetEntry(path);
        if (entry is DirectoryEntry directoryEntry)
        {
            directoryEntry.Unload(recursive);
        }
    }
}