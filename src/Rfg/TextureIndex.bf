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

            PackfileV3 vpp = null;
            switch (PackfileVFS.GetPackfile(vppName))
            {
                case .Ok(PackfileV3 packfile):
                    vpp = packfile;
                case .Err(StringView err):
                    Logger.Error("Failed to find packfile '{}' for texture indexing", vppName);
                    return .Err(err);
            }

            PackfileTextureIndex vppIndex = new .(vppName);
            bool success = false;
            defer { if (!success) delete vppIndex; }

            for (int i in 0 ..< vpp.Entries.Count)
            {
                StringView entryName = vpp.EntryNames[i];
                String ext = Path.GetExtension(entryName, .. scope .())..ToLower();
                if (ext == ".cpeg_pc" || ext == ".cvbm_pc") //Index pegs
                {
                    switch (vpp.ReadSingleFile(entryName))
                    {
                        case .Ok(u8[] cpuFileBytes):
                            defer delete cpuFileBytes;
                            if (IndexPeg(cpuFileBytes, entryName, vppIndex) case .Err(StringView err))
                            {
                                Logger.Error("Failed to index {}/{}. Error: {}", vpp.Name, entryName);
                                return .Err(err);
                            }

                        case .Err(StringView err):
                            Logger.Error("Failed to extract {}/{}.", vpp.Name, entryName);
                            return .Err(err);
                    }
                }
                else if (ext == ".str2_pc") //Index pegs in asset containers
                {
                    //Extract and parse str2_pc, then load all its subfiles
                    MemoryFileList subfiles = null;
                    defer { if (subfiles != null) delete subfiles; }
                    switch (vpp.ReadSingleFile(entryName))
                    {
                        case .Ok(u8[] str2Bytes):
                            defer delete str2Bytes;
                            PackfileV3 str2 = scope .(new ByteSpanStream(str2Bytes), entryName);
                            if (str2.ReadMetadata() case .Err(StringView err))
                            {
                                Logger.Error("Failed to parse {} for texturing indexing. Error: {}", entryName, err);
                                return .Err(err);
                            }

                            switch (str2.ExtractSubfilesToMemory())
                            {
                                case .Ok(MemoryFileList files):
                                    subfiles = files;
                                case .Err(StringView err):
                                    Logger.Error("Failed to extract {} subfiles for texture indexing. Error: {}", entryName, err);
                                    return .Err(err);
                            }
                        case .Err(StringView err):
                            Logger.Error("Failed to extract {} for texture indexing. Error: {}", entryName, err);
                            return .Err(err);
                    }

                    PackfileTextureIndex str2Index = new .(scope $"{vpp.Name}/{entryName}");
                    for (var file in subfiles.Files)
                    {
                        String subfileExt = Path.GetExtension(file.Name, .. scope .())..ToLower();
                        if (subfileExt == ".cpeg_pc" || subfileExt == ".cvbm_pc")
                        {
                            if (IndexPeg(file.Data, file.Name, str2Index) case .Err(StringView err))
                            {
                                Logger.Error("Failed to index {}/{}. Error: {}", vpp.Name, entryName);
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

            success = true;
            return .Ok;
        }

        private static Result<void, StringView> IndexPeg(Span<u8> cpuFileBytes, StringView pegName, PackfileTextureIndex packfileIndex)
        {
            PegV10 peg = scope .();
            switch (peg.Load(cpuFileBytes, .Empty, true))
            {
                case .Ok:
                    PegTextureIndex pegIndex = new .();
                    pegIndex.Name.Set(pegName);
                    for (int i in 0 ..< peg.Entries.Length)
                    {
                        if (peg.GetEntryName(i) case .Ok(char8* entryName))
                        {
                            pegIndex.SubtextureNameHashes.Add(StringView(entryName).GetHashCode());
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

        public static void TestIndexAllVpps()
        {
            Stopwatch totalTimer = scope .(true);
            for (PackfileV3 packfile in PackfileVFS.Packfiles)
            {
                Stopwatch timer = scope .(true);
                switch (IndexVpp(packfile.Name))
                {
                    case .Ok:
                        Logger.Info("Done indexing textures in {}. Took {}s", packfile.Name, timer.Elapsed.TotalSeconds);
                    case .Err(StringView err):
                        Logger.Error("Failed to index {}. Error: {}", packfile.Name, err);
                }
            }
            Logger.Info("Finished indexing textures for all packfiles in the data folder. Took {}s", totalTimer.Elapsed.TotalSeconds);
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
            int tgaNameHash = tgaName.GetHashCode();
            for (PackfileTextureIndex packfileIndex in _packfileIndices)
            {
                for (PegTextureIndex pegIndex in packfileIndex.Pegs)
                {
                    for (int hash in pegIndex.SubtextureNameHashes)
                    {
                        if (hash == tgaNameHash)
                        {
                            //Sets 'path' to the path of the peg file on success. PackfileVFS can extract the peg with that path.
                            path.Set(scope $"{packfileIndex.Path}/{pegIndex.Name}");
                            return .Ok;
                        }
                    }
                }    
            }

            return .Err;
        }
	}
}