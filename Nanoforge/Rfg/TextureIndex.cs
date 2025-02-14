using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using Nanoforge.FileSystem;
using RFGM.Formats.Peg;
using RFGM.Formats.Peg.Models;
using Serilog;

namespace Nanoforge.Rfg;

//Indexes packfiles so textures can be searched for quickly. Can search by the name of their peg or the name of a sub-texture in a peg (e.g. dust_basesand_n.tga)
//Searching by sub-texture name can be expensive since it has to extract the entire peg and parse it.
//This is especially slow on demand if many pegs are in one str2_pc. Since they get extracted and decompressed multiple times.
//This design is a bit different from the C++ one. It indexes every time NF runs instead of being pregenerated. That way it can handle custom and modded map files.
//It only indexes vpps when requested to keep startup times quick.
public static class TextureIndex
{
    private static object _indexLock = new();
    private static List<PackfileTextureIndex> _packfileIndices = new();

    private class PackfileTextureIndex(string path)
    {
        public readonly string Path = path;
        public List<PegTextureIndex> Pegs = new();
    }

    private class PegTextureIndex(string name)
    {
        public string Name = name;
        //TODO: If we're ever looking for memory savings this could hold the hash of the name instead. For mp_common this netted ~20MB ram savings. Though that has many textures.
        //TODO: Alternatively, add an option to clear the index after each import or limit how many packfiles can be indexed at once.
        public List<string> SubTextureNames = new();
    }

    //Recursively index a vpp_pc
    public static bool IndexVpp(string vppName)
    {
        try
        {
            if (IsIndexed(vppName))
                return true;

            EntryBase? entry = PackfileVFS.GetEntry($"//data/{vppName}/"); //TODO: De-hardcode the data folder mount point
            if (entry == null)
            {
                Log.Error($"Texture index could not find {vppName} for indexing.");
                return false;
            }
            if (entry is not DirectoryEntry directoryEntry)
            {
                Log.Error($"Texture index tried index {vppName}. But the entry in PackfileVFS is not a directory.");
                return false;                
            }
            
            bool result = IndexDirectory(directoryEntry);
            return result;
        }
        catch (Exception ex)
        {
            Log.Error($"Error in TextureIndex.IndexVpp(): {ex.Message}");
            return false;
        }
    }

    public static bool IndexDirectory(DirectoryEntry directory)
    {
        try
        {
            if (IsIndexed(directory))
                return true;
            
            Log.Information($"Indexing textures in {directory.Name}...");
            
            string indexName;
            if (directory.Name.EndsWith(".str2_pc") && directory.Parent is not null)
                indexName = $"{directory.Parent.Name}/{directory.Name}";
            else
                indexName = directory.Name;
            
            PackfileTextureIndex index = new(indexName);
            foreach (var entry in directory)
            {
                string ext = Path.GetExtension(entry.Name).ToLower();
                switch (ext)
                {
                    //Index top level pegs
                    case ".cpeg_pc":
                    case ".cvbm_pc":
                        IndexPeg((FileEntry)entry, index);
                        break;

                    //Index pegs in asset containers
                    case ".str2_pc":
                    {
                        DirectoryEntry container = (entry as DirectoryEntry)!;
                        IndexDirectory(container);
                        break;
                    }
                }
            }

            lock (_indexLock)
            {
                _packfileIndices.Add(index);
            }
            
            Log.Information($"Done indexing textures in {directory.Name}!");

            return true;
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Failed to index textures in PackfileVFS directory '{directory.Name}'");
            return false;
        }
    }

    private static bool IndexPeg(FileEntry cpuFileEntry, PackfileTextureIndex packfileIndex)
    {
        try
        {
            DirectoryEntry container = (cpuFileEntry.Parent as DirectoryEntry)!;
        
            string ext = Path.GetExtension(cpuFileEntry.Name).ToLower();
            string gpuFileExt = ext switch
            {
                ".cpeg_pc" => ".gpeg_pc",
                ".cvbm_pc" => ".gvbm_pc",
                _ => throw new Exception($"Unsupported texture extension: {ext}")
            };
        
            string gpuFileName = $"{Path.GetFileNameWithoutExtension(cpuFileEntry.Name)}{gpuFileExt}";
            EntryBase? gpuFileEntry = container.Entries.Find(entry => entry.Name == gpuFileName);
            if (gpuFileEntry == null || gpuFileEntry.GetType() != typeof(FileEntry))
            {
                Log.Error($"TextureIndex.IndexPeg() failed. Couldn't find gpu file '{gpuFileName}' in {container.Name}");
                return false;
            }
        
            using Stream? cpuFile = cpuFileEntry.OpenStream();
            using Stream? gpuFile = (gpuFileEntry as FileEntry)!.OpenStream();
            if (cpuFile == null)
            {
                Log.Error($"Failed to open stream for cpu file '{cpuFileEntry.Name}' when indexing textures.");
                return false;
            }
            if (gpuFile == null)
            {
                Log.Error($"Failed to open stream for gpu file '{gpuFileName}' when indexing textures.");
                return false;
            }
            
            PegReader reader = new();
            LogicalTextureArchive archive = reader.Read(cpuFile, gpuFile, cpuFileEntry.Name, new CancellationToken());
            PegTextureIndex pegIndex = new(cpuFileEntry.Name);
            foreach (LogicalTexture texture in archive.LogicalTextures)
            {
                pegIndex.SubTextureNames.Add(texture.Name);
            }
            
            packfileIndex.Pegs.Add(pegIndex);
            
            return true;
        }
        catch (Exception ex)
        {
            Log.Error($"Error in TextureIndex.IndexPeg(): {ex.Message}");
            return false;
        }
    }

    public static bool IsIndexed(string packfileName)
    {
        lock (_indexLock)
        {
            foreach (PackfileTextureIndex index in _packfileIndices)
                if (index.Path.Equals(packfileName, StringComparison.OrdinalIgnoreCase))
                    return true;
        }

        return false;
    }

    public static bool IsIndexed(DirectoryEntry directory)
    {
        string indexName;
        if (directory.Name.EndsWith(".str2_pc") && directory.Parent is not null)
            indexName = $"{directory.Parent.Name}/{directory.Name}";
        else
            indexName = directory.Name;
        
        lock (_indexLock)
        {
            foreach (PackfileTextureIndex index in _packfileIndices)
                if (index.Path.Equals(indexName, StringComparison.OrdinalIgnoreCase))
                    return true;
        }

        return false;
    }

    //Get the path of the cpeg_pc/cvbm_pc file that has a subtexture with Name == tgaName
    public static string? GetTexturePegPath(string tgaName)
    {
        lock (_indexLock)
        {
            foreach (PackfileTextureIndex packfileIndex in _packfileIndices)
            {
                foreach (PegTextureIndex pegIndex in packfileIndex.Pegs)
                {
                    foreach (string name in pegIndex.SubTextureNames)
                    {
                        if (name.Equals(tgaName, StringComparison.OrdinalIgnoreCase))
                        {
                            return $"//data/{packfileIndex.Path}/{pegIndex.Name}"; //TODO: De-hardcode the data folder mount point
                        }
                    }
                }
            }
        }

        return null;
    }
}