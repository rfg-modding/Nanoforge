using Common;
using System;
using Nanoforge.FileSystem;
using RfgTools.Formats;
using Nanoforge.Misc;
using System.IO;
using System.Threading;
using System.Collections;
using RfgTools.Formats.Textures;
using Common.IO;
using System.Diagnostics;
using System.Linq;

namespace Nanoforge.Rfg
{
    //Indexes packfiles so textures can be searched for quickly. Can search by the name of their peg or the name of a sub-texture in a peg (e.g. dust_basesand_n.tga)
    //Searching by sub-texture name can be expensive since it has to extract the entire peg and parse it.
	//This is especially slow on demand if many pegs are in one str2_pc. Since they get extracted and decompressed multiple times.
    //This design is a bit different from the C++ one. It indexes every time NF runs instead of being pregenerated. That way it can handle custom and modded map files.
    //It only indexes vpps when requested to keep startup times quick.
	public static class TextureIndex
	{
        private static append Monitor _indexLock;
        public static append List<PackfileTextureIndex> _packfileIndices ~ClearAndDeleteItems(_);

        public class PackfileTextureIndex
        {
            public readonly append String Path; //For vpps this is just the name. For str2s this includes the parents vpp name. E.g. mp_crescent.vpp_pc/ns_base.str2_pc
            public append List<PegTextureIndex> Pegs ~ClearAndDeleteItems(_);

            public this(StringView path)
            {
                Path.Set(path);
            }
        }
        public class PegTextureIndex
        {
            public append String Name;
            public append List<int> SubtextureNameHashes;
        }

        //Recursively index a vpp_pc
        public static Result<void, StringView> IndexVpp(StringView vppName)
        {
            if (IsPackfileIndexed(vppName))
                return .Err("This vpp was already indexed");

            Logger.Info("Indexing {}...", vppName);

            PackfileTextureIndex vppIndex = new .(vppName);
            bool success = false;
            defer { if (!success) delete vppIndex; }

            for (var entry in PackfileVFS.Enumerate(scope $"//data/{vppName}/"))
            {
                String ext = Path.GetExtension(entry.Name, .. scope .())..ToLower();
                if (ext == ".cpeg_pc" || ext == ".cvbm_pc") //Index pegs
                {
                    if (IndexPeg(entry as PackfileVFS.FileEntry, vppIndex) case .Err(StringView err))
                    {
                        Logger.Error("Failed to index {}/{}. Error: {}", vppName, entry.Name);
                        return .Err(err);
                    }
                }
                else if (ext == ".str2_pc") //Index pegs in asset containers
                {
                    //Extract and parse str2_pc, then load all its subfiles
                    PackfileVFS.DirectoryEntry container = entry as PackfileVFS.DirectoryEntry;
                    PackfileTextureIndex str2Index = new .(scope $"{vppName}/{entry.Name}");
                    for (var subEntry in container)
                    {
                        if (!subEntry.IsFile)
                            continue;

                        String subfileExt = Path.GetExtension(subEntry.Name, .. scope .())..ToLower();
                        if (subfileExt == ".cpeg_pc" || subfileExt == ".cvbm_pc")
                        {
                            if (IndexPeg(subEntry as PackfileVFS.FileEntry, str2Index) case .Err(StringView err))
                            {
                                Logger.Error("Failed to index {}/{}. Error: {}", vppName, entry.Name, err);
                                delete str2Index;
                                return .Err(err);
                            }
                        }
                    }

                    if (str2Index.Pegs.Count > 0)
                    {
                        ScopedLock!(_indexLock);
                        _packfileIndices.Add(str2Index);
                    }
                    else
                    {
                        delete str2Index;
                    }    
                }
            }

            //Always add the vppIndex to the list even if it has no pegs. That way IsPackfileIndexed() detects it
            ScopedLock!(_indexLock);
            _packfileIndices.Add(vppIndex);

            Logger.Info("Done indexing {}", vppName);
            success = true;
            return .Ok;
        }

        private static Result<void, StringView> IndexPeg(PackfileVFS.FileEntry entry, PackfileTextureIndex packfileIndex)//(Span<u8> cpuFileBytes, StringView pegName, PackfileTextureIndex packfileIndex)
        {
            PegV10 peg = scope .();
            Result<u8[]> cpuFileBytes = PackfileVFS.ReadAllBytes(entry);
            if (cpuFileBytes case .Err)
            {
                Logger.Error("Texture indexer failed to read bytes for {}", entry.Name);
                return .Err("Failed to read peg cpu file bytes");
            }
            defer delete cpuFileBytes.Value;

            switch (peg.Load(cpuFileBytes.Value, .Empty, true))
            {
                case .Ok:
                    PegTextureIndex pegIndex = new .();
                    pegIndex.Name.Set(entry.Name);
                    for (int i in 0 ..< peg.Entries.Length)
                    {
                        if (peg.GetEntryName(i) case .Ok(char8* entryName))
                        {
                            pegIndex.SubtextureNameHashes.Add(StringView(entryName).ToLower(.. scope .()).GetHashCode());
                        }
                        else
                        {
                            delete pegIndex;
                            return .Err("Failed to get peg entry name");
                        }
                    }

                    packfileIndex.Pegs.Add(pegIndex);
                case .Err(StringView err):
                    return .Err(err);
            }

            return .Ok;
        }

        public static bool IsPackfileIndexed(StringView path)
        {
            ScopedLock!(_indexLock);
            for (PackfileTextureIndex index in _packfileIndices)
                if (index.Path.Equals(path, .OrdinalIgnoreCase))
                    return true;

            return false;
        }

        public static Result<void> GetTexturePegPath(StringView tgaName, String path)
        {
            int tgaNameHash = tgaName.ToLower(.. scope .()).GetHashCode();
            for (PackfileTextureIndex packfileIndex in _packfileIndices)
            {
                for (PegTextureIndex pegIndex in packfileIndex.Pegs)
                {
                    for (int hash in pegIndex.SubtextureNameHashes)
                    {
                        if (hash == tgaNameHash)
                        {
                            //Sets 'path' to the path of the peg file on success. PackfileVFS can extract the peg with that path.
                            path.Set(scope $"//data/{packfileIndex.Path}/{pegIndex.Name}");
                            return .Ok;
                        }
                    }
                }    
            }

            return .Err;
        }
	}
}