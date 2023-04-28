using Common;
using System;
using System.Collections;
using System.IO;
using RfgTools.Formats;
using Nanoforge.Misc;
using System.Linq;
using Common.IO;
using Zlib;

namespace Nanoforge.FileSystem
{
    //VFS implementation for RFG packfiles (.vpp_pc & .str2_pc version 3). Tracks all the vpps in the specified data folder.
    //NOT thread safe. You better make some improvements to this before you try to use it on multiple threads simultaneously.
	public static class PackfileVFS
	{
        private static DirectoryEntry Root = null ~ DeleteIfSet!(_); //Root node of the filesystem. Has the same name as the mount point.
        public static readonly append String DirectoryPath;
        public static readonly append String Mount; //Directory this FS is mounted to in the VFS

        public static this()
        {
            Root = null; //Note: Odd bug where Root wouldn't be null the first time InitFromDirectory() is called. Caused DeleteIfSet to crash since it'd try deleting a fake object
        }

        //Parse all vpp_pc files in this directory. They'll all be accessible as sub-directories of this filesystem
        [Trace(.Enter | .Exit | .RunTime)]
        public static void InitFromDirectory(StringView mount, StringView directoryPath)
        {
            Mount.Set(mount);
            DirectoryPath.Set(directoryPath);
            DeleteIfSet!(Root);
            Root = new .(mount);
            for (var entry in Directory.EnumerateFiles(DirectoryPath, "*.vpp_pc"))
            {
                String packfilePath = entry.GetFilePath(.. scope .());
                if (MountPackfile(packfilePath) case .Err(StringView err))
                {
                    Logger.Error("RfgFileSystem failed to mount {} in the VFS. Error: {}", packfilePath, err);
                    continue;
                }
            }
        }

        public static Result<void, StringView> MountPackfile(StringView path, bool loadIntoMemory = false)
        {
            PackfileV3 packfile = new .(path);
            defer delete packfile;
            if (packfile.ReadMetadata() case .Err(StringView err))
                return .Err(err);

            //Make a directory for the packfile
            DirectoryEntry packfileEntry = new .(packfile.Name, packfile.Compressed, packfile.Condensed);
            packfileEntry.Parent = Root;
            packfileEntry.[Friend]DataBlockOffset = packfile.[Friend]_dataBlockOffset;
            packfileEntry.[Friend]DataOffset = i64.MaxValue;
            packfileEntry.[Friend]CompressedSize = u32.MaxValue;
            packfileEntry.[Friend]Size = u32.MaxValue;

            for (int entryIndex in 0 ..< packfile.Entries.Count)
            {
                ref PackfileV3.Entry entry = ref packfile.Entries[entryIndex];
                StringView entryName = packfile.EntryNames[entryIndex];
                String entryExtension = Path.GetExtension(entryName, .. scope .())..ToLower();

                if (entryExtension == ".str2_pc") //Container. Create a directory with the name of the str2 and add its subfiles to it
                {
                    DirectoryEntry containerEntry = new .(entryName);
                    containerEntry.Parent = packfileEntry;
                    containerEntry.[Friend]DataOffset = (i64)entry.DataOffset;
                    containerEntry.[Friend]CompressedSize = entry.CompressedDataSize;
                    containerEntry.[Friend]Size = entry.DataSize;

                    if (packfile.Compressed) //Packfile is compressed, so we need to extract the whole str2 (much slower)
                    {
                        Logger.Error("{} is compressed and contains str2_pc files. Can't parse the str2_pc. Please repack that vpp_pc without compression and restart Nanoforge or it won't work properly.", Path.GetFileName(path, .. scope .()));
                        return .Err("A vpp_pc containing str2_pc files is compressed. PackfileVFS doesn't support that because the game doesn't.");
                    }
                    else //Packfile isn't compressed. Instead of extracting the whole str2 just open a filestream in the vpp and seek to the str2 header
                    {
                        PackfileV3 container = new .(path, packfile.[Friend]_dataBlockOffset + (int)entry.DataOffset);
                        defer delete container;
                        if (container.ReadMetadata() case .Err(StringView err))
                            return .Err(err);

                        containerEntry.[Friend]DataBlockOffset = container.[Friend]_dataBlockOffset;
                        containerEntry.[Friend]Compressed = container.Compressed;
                        containerEntry.[Friend]Condensed = container.Condensed;
                        for (int j in 0 ..< container.Entries.Count)
                        {
                            ref PackfileV3.Entry containerSubfile = ref container.Entries[j];
                            StringView containerSubfileName = container.EntryNames[j];
                            FileEntry fileEntry = new .(containerSubfileName);
                            fileEntry.Parent = containerEntry;
                            fileEntry.[Friend]DataOffset = (i64)containerSubfile.DataOffset;
                            fileEntry.[Friend]CompressedSize = containerSubfile.CompressedDataSize;
                            fileEntry.[Friend]Size = containerSubfile.DataSize;
                            containerEntry.Entries.Add(fileEntry);
                        }
                    }

                    packfileEntry.Entries.Add(containerEntry);
                }
                else //Plain file, add to packfiles root directory
                {
                    FileEntry fileEntry = new .(entryName);
                    fileEntry.Parent = packfileEntry;
                    fileEntry.[Friend]DataOffset = (i64)entry.DataOffset;
                    fileEntry.[Friend]CompressedSize = entry.CompressedDataSize;
                    fileEntry.[Friend]Size = entry.DataSize;
                    packfileEntry.Entries.Add(fileEntry);
                }
            }

            Root.Entries.Add(packfileEntry);
            return .Ok;
        }

        //Get VFS entry from a path
        private static Result<EntryBase> GetEntry(StringView targetPath)
        {
            StringView path = targetPath;

            //Strip mount point. E.g. //data/
            if (!path.StartsWith(Mount))
            {
                Logger.Error("Invalid path '{}' passed to VFS. Path must start with the mount point '{}'", targetPath, Mount);
                return .Err;
            }
            path.RemoveFromStart(Mount.Length);
            if (path.Length == 0)
                return Root;

            DirectoryEntry directory = Root;
            while (true && path.Length > 0)
            {
                int nextDelimiter = path.IndexOf('/');
                if (nextDelimiter == -1)
                {
                    //No more slashes. We hit the end of the path on a file
                    StringView part = path;
                    FileEntry result = directory.Entries.Select((entry) => entry)
						                                .Where((entry) => entry.Name.Equals(part, .OrdinalIgnoreCase) && entry.IsFile)
						                                .FirstOrDefault() as FileEntry;
                    if (result != null)
                        return .Ok(result);
                    else
                        return .Err;
                }
                else if (nextDelimiter == path.Length - 1)
                {
                    //Slash on the end of the path. We hit the end of the path on a directory
                    StringView part = path..RemoveFromEnd(1);
                    DirectoryEntry result = directory.Entries.Select((entry) => entry)
                    	                                .Where((entry) => entry.Name.Equals(part, .OrdinalIgnoreCase) && entry.IsDirectory)
                    	                                .FirstOrDefault() as DirectoryEntry;
                    if (result != null)
                        return .Ok(result);
                    else
                        return .Err;
                }
                else
                {
                    //Slash in the middle of the path. Find the next entry and proceed
                    StringView part = path.Substring(0, nextDelimiter);
                    path.RemoveFromStart(nextDelimiter + 1);
                    DirectoryEntry result = directory.Entries.Select((entry) => entry)
	                    .Where((entry) => entry.Name.Equals(part, .OrdinalIgnoreCase) && entry.IsDirectory)
	                    .FirstOrDefault() as DirectoryEntry;

                    if (result == null)
                        return .Err;
                    else
                        directory = result;
                }
            }

            return .Err;
        }

        public static DirectoryEntry.Enumerator Enumerate(StringView path)
        {
            if (GetEntry(path) case .Ok(EntryBase entry) && entry.IsDirectory)
                return (entry as DirectoryEntry).GetEnumerator();
            else
                return DirectoryEntry.Enumerator.Empty;
        }

        public static bool Exists(StringView path)
        {
            return GetEntry(path) case .Ok;
        }

        public static Result<Stream> OpenFile(StringView path)
        {
            if (GetEntry(path) case .Ok(EntryBase entryBase))
            {
                return OpenFile(entryBase);
            }
            else
            {
                return .Err;
            }
        }

        //Open a file entry. Caller is responsible for deleting the stream. 
        //Eventually write support will be added for file entries. That will auto update vpps and str2s, removing any need for directly opening them.
        public static Result<Stream> OpenFile(EntryBase entryBase)
        {
            if (entryBase.IsDirectory || entryBase.Parent == null)
                return .Err;

            FileEntry entry = (entryBase as FileEntry);
            if (entry.Preloaded)
            {
                return new ByteSpanStream(entry.[Friend]_data); //TODO: MAKE A PROPER FUNCTION FOR THIS + HANDLE REFERENCE COUNTING
            }

            DirectoryEntry parent = (entry.Parent as DirectoryEntry);
            switch (Path.GetExtension(parent.Name, .. scope .()))
            {
                case ".vpp_pc":
                    if (parent.Compressed && parent.Condensed)
                    {
                        Logger.Error("Tried reading path {} from {}, which is compressed and condensed. PackfileVFS doesn't support reading from vpps packed that way. Please repack the vpp with a different set of flags. Just compressed, just condensed, or neither works fine. Compressed + condensed is a special case that works differently.", entry.Name, parent.Name);
                        return .Err;
                    }
                    else if (parent.Compressed)
                    {
                        //File is in a compressed vpp. Open file stream in vpp, seek to data, decompress
                        FileStream stream = new FileStream();
                        defer delete stream;
                        stream.Open(scope $"{DirectoryPath}/{parent.Name}", .Read, .Read);
                        stream.Seek(parent.DataBlockOffset + entry.DataOffset);
                        u8[] compressedBytes = new u8[entry.CompressedSize];
                        u8[] decompressedBytes = new u8[entry.Size];
                        defer delete compressedBytes;
                        defer delete decompressedBytes;
                        if (stream.TryRead(compressedBytes) case .Err)
                            return .Err;
                        if (Zlib.Inflate(compressedBytes, decompressedBytes) != .Ok)
                            return .Err;

                        //TODO: Might get slight perf boost by storing these files in entry._data. Would likely need reference counting and period checks to know when its safe to free the memory. Benchmark before committing!
                        return new MemoryStream(decompressedBytes.ToList(.. new .()), true);
                    }
                    else
                    {
                        //TODO: Consider having alternative version of FileStream that only sees part of the file. Current version can seek into other parts of the packfile that're outside of the subfile.
                        FileStream stream = new FileStream();
                        stream.Open(scope $"{DirectoryPath}/{parent.Name}", .Read, .Read);
                        stream.Seek(parent.DataBlockOffset + entry.DataOffset);
                        return stream;
                    }
                case ".str2_pc":
                    //File is in a container. Need to decompress the whole containers data block and extract the single entries data. In this case its recommended to preload to whole container instead.
                    DirectoryEntry containerParent = entry.Parent.Parent != null ? (DirectoryEntry)entry.Parent.Parent : null;
                    if (containerParent == null)
                        return .Err;
                    if (containerParent.Compressed)
                    {
                        //Note: Simplification of the code since since the game has crashed so far any time we've compressed vpps with containers in them. No point in supporting cases the game doesn't support.
                        Logger.Error("Failed to open stream to container subfile. The vpp_pc holding the str2_pc file is compressed. Please repack {} without compression enabled.", containerParent.Name);
                        return .Err;
                    }

                    PackfileV3 container = new .(scope $"{DirectoryPath}/{containerParent.Name}", containerParent.DataBlockOffset + parent.DataOffset);
                    defer delete container;
                    if (container.ReadMetadata() case .Err(StringView err))
                    {
                        Logger.Error("Failed to parse {} while opening {}. Error: {}", entry.Parent.Name, entry.Name, err);
						return .Err;
                    }

                    switch (container.ReadSingleFile(entry.Name))
                    {
                        case .Ok(u8[] bytes):
                            defer delete bytes;
                            return new MemoryStream(bytes.ToList(.. new .()), true);
                        case .Err(StringView err):
                            Logger.Error("Failed to read {} from {}. Error: {}", entry.Name, container.Name, err);
                            return .Err;
                    }


                default:
                    return .Err;
            }
        }

        /*private static Result<u8[]> LoadSingleFile(StringView path)
        {
            switch (GetEntry(path))
            {
                case .Ok(EntryBase entryBase):
                    if (entryBase.IsFile)
                        return LoadSingleFile(entryBase as FileEntry);
                    else
                        return .Err;
                case .Err:
                    return .Err;
            }
        }*/

        /*private static Result<u8[]> LoadSingleFile(FileEntry entry)
        {

        }*/

        public static Result<u8[]> ReadAllBytes(StringView path)
        {
            if (GetEntry(path) case .Ok(EntryBase entry) && entry.IsFile)
                return ReadAllBytes(entry as FileEntry);
            else
                return .Err;
        }

        public static Result<u8[]> ReadAllBytes(FileEntry entry)
        {
            switch (OpenFile(entry))
            {
                case .Ok(Stream stream):
                    defer delete stream;
                    u8[] bytes = new u8[entry.Size];
                    if (stream.TryRead(bytes) case .Ok)
                    {
                    	return bytes;
                    }
                    else
                    {
                        delete bytes;
                        return .Err;
                    }
                case .Err:
                    return .Err;
            }
        }

        public static void GetMountName(String inMountName)
        {
            inMountName.Set(Mount);
        }

        //Load files in this directory into memory. If recursive == true, then the subfiles of subdirectories will also be loaded.
        public static void PreloadDirectory(StringView path, bool recursive = false)
        {
            if (GetEntry(path) case .Ok(EntryBase entry))
            {
                if (entry.IsDirectory)
                {
                    PreloadDirectory(entry as DirectoryEntry, recursive);
                }
            }
            else
            {
                Logger.Warning("Failed to locate {} for preloading", path);
            }
        }

        public static void PreloadDirectory(DirectoryEntry directory, bool recursive = false)
        {
            Logger.Info("Preloading {}...", directory.Name);
            defer Logger.Info("Done preloading {}", directory.Name);
            switch (Path.GetExtension(directory.Name, .. scope .()))
            {
                case ".vpp_pc":
                    if (directory.Compressed && directory.Condensed)
                    {
                        Logger.Error("Tried pre-loading a packfile that's compressed + condensed ({}). Not currently supported. Please repack the vpp with a different set of flags, restart Nanoforge, and try again.", directory.Name);
                        return;
                    }

                    FileStream stream = new FileStream();
                    defer delete stream;
                    stream.Open(scope $"{DirectoryPath}/{directory.Name}", .Read, .Read);
                    for (var entry in directory)
                    {
                        if (!entry.IsFile)
                            continue;

                        FileEntry file = entry as FileEntry;
                        stream.Seek(directory.DataBlockOffset + file.DataOffset);
                        if (directory.Compressed)
                        {
                            u8[] compressed = new u8[entry.CompressedSize];
                            u8[] decompressed = new u8[entry.Size];
                            defer delete compressed;
                            if (stream.TryRead(compressed) case .Err)
                            {
                                delete decompressed;
                                return;
                            }

                            if (Zlib.Inflate(compressed, decompressed) != .Ok)
                            {
                                delete decompressed;
                                return;
                            }

                            file.[Friend]_data = decompressed;
                        }
                        else
                        {
                            u8[] bytes = new u8[entry.Size];
                            if (stream.TryRead(bytes) case .Err)
                            {
                                delete bytes;
                                return;
                            }
                            file.[Friend]_data = bytes;
                        }
                    }


                    //Preload subdirs if recursive
                    if (recursive)
                        for (var entry in directory)
	                        if (entry.IsDirectory)
	                            PreloadDirectory(entry as DirectoryEntry);

                case ".str2_pc":
                    //File is in a container. Need to decompress the whole containers data block and extract the single entries data. In this case its recommended to preload to whole container instead.
                    DirectoryEntry containerParent = directory.Parent as DirectoryEntry;
                    if (containerParent == null)
                        return;
                    if (containerParent.Compressed)
                    {
                        //Note: Simplification of the code since since the game has crashed so far any time we've compressed vpps with containers in them. No point in supporting cases the game doesn't support.
                        Logger.Error("Failed to open stream to container subfile. The vpp_pc holding the str2_pc file is compressed. Please repack {} without compression enabled.", containerParent.Name);
                        return;
                    }

                    PackfileV3 container = new .(scope $"{DirectoryPath}/{containerParent.Name}", containerParent.DataBlockOffset + directory.DataOffset);
                    defer delete container;
                    if (container.ReadMetadata() case .Err(StringView err))
                    {
                        Logger.Error("Failed to parse {} while opening {}. Error: {}", directory.Parent.Name, directory.Name, err);
                    	return;
                    }

                    MemoryFileList subfiles = container.ExtractSubfilesToMemory().GetValueOrDefault(null);
                    defer { DeleteIfSet!(subfiles); }
                    if (subfiles == null)
                        return;

                    for (MemoryFileList.MemoryFile file in subfiles.Files)
                    {
                        for (var entry in directory)
                        {
                            if (entry.IsFile && entry.Name == file.Name)
                            {
                                u8[] bytes = new u8[file.Data.Length];
                                Internal.MemCpy(bytes.Ptr, file.Data.Ptr, file.Data.Length);
                                (entry as FileEntry).[Friend]_data = bytes;
                            }
                        }
                    }
            }
        }

        public static void UnloadDirectory(StringView path, bool recursive = false)
        {
            if (GetEntry(path) case .Ok(EntryBase entry))
            {
                if (entry.IsDirectory)
                {
                    UnloadDirectory(entry as DirectoryEntry, recursive);
                }
            }
        }

        public static void UnloadDirectory(DirectoryEntry directory, bool recursive = false)
        {
            Logger.Info("Unloading {}...", directory.Name);
            for (EntryBase entry in directory)
            {
                if (entry.IsFile && (entry as FileEntry).[Friend]_data != null)
				{
                    //TODO: This might need to be improved with some kind of reference counting eventually. It suffices for the current use case of single threaded map imports. But may not work in more complex cases.
                    FileEntry file = entry as FileEntry;
                    DeleteIfSet!(file.[Friend]_data);
				}
                else if (entry.IsDirectory && recursive)
                {
                    //Unload subdirs if recursive
                    PreloadDirectory(entry as DirectoryEntry);
                }
            }
        }

        public class EntryBase
        {
            public EntryBase Parent = null;
            public readonly append String Name;
            public i64 DataBlockOffset { get; private set; } = 0;
            public i64 DataOffset { get; private set; } = 0;
            public u32 Size { get; private set; } = 0;
            public u32 CompressedSize { get; private set; } = 0;

            public this(StringView name)
            {
                Name.Set(name);
            }

            public virtual bool IsDirectory => false;
            public virtual bool IsFile => false;
        }

        //Packfile or container (vpp_pc/str2_pc)
        public class DirectoryEntry : EntryBase, IEnumerable<EntryBase>
        {
            public append List<EntryBase> Entries ~ClearAndDeleteItems(_);
            public override bool IsDirectory => true;
            public override bool IsFile => false;
            public bool Compressed { get; private set; } = false;
            public bool Condensed  { get; private set; } = false;

            public this(StringView name, bool compressed = false, bool condensed = false) : base(name)
            {
                Compressed = compressed;
                Condensed = condensed;
            }

            public DirectoryEntry.Enumerator GetEnumerator()
            {
                return Enumerator(Entries.GetEnumerator());
            }

            public struct Enumerator : IEnumerator<EntryBase>
            {
                public static DirectoryEntry.Enumerator Empty = .(true);

                List<EntryBase>.Enumerator _enumerator;
                bool _empty = false;

                public this(List<EntryBase>.Enumerator enumerator)
                {
                    _enumerator = enumerator;
                    _empty = false;
                }

                //Used by static Empty. For functions like RfgFileSystem.Enumerate() in the case where the input path is invalid and we need the enumerator to work but be empty.
                private this(bool empty)
                {
                    _enumerator = default;
                    _empty = empty;
                }

                Result<EntryBase> IEnumerator<EntryBase>.GetNext() mut
                {
                    if (_empty)
                        return .Err;

                    return _enumerator.GetNext();
                }
            }
        }

        //Primitive file - anything that isn't a vpp_pc or str2_pc
        public class FileEntry : EntryBase
        {
            public override bool IsDirectory => false;
            public override bool IsFile => true;
            private u8[] _data = null;
            public bool Preloaded => _data != null;

            public this(StringView name) : base(name)
            {

            }
        }
	}
}