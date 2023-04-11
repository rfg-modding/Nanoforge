using System.Collections;
using RfgTools.Formats;
using Common;
using System.IO;
using System;
using System.Linq;
using Nanoforge.Misc;
using Common.IO;

namespace Nanoforge.FileSystem
{
    ///RFG packfile interface
    ///NOTE: This API sucks and will be rewritten in the near future. It's a port from the C++ version. This was originally written when Nanoforge was just a file viewer.
    ///      It's insufficient for the new goals of Nanoforge of being a full modding suite. It will be rewritten after adding the map editor, but before adding other features.
    ///      I'm using the existing code initially to save time. I want to get the rewrite public ASAP so improvements to the map editor can be started again.
    ///      Doing all the code rewrites up front would likely delay that by months. It's better to get early feedback IMO.
	public static class PackfileVFS
	{
        private static String _dataFolder = new .() ~delete _;
        public static List<PackfileV3> Packfiles = new .() ~DeleteContainerAndItems!(_);

        ///Scan all packfiles in the data folder
        public static void SetDataFolder(StringView dataFolderPath)
        {
            if (!Directory.Exists(dataFolderPath))
			{
                //TODO: Add error logging here
                return;
            }

            //Clear packfiles
            _dataFolder.Set(dataFolderPath);
            ClearAndDeleteItems(Packfiles);

            //Scan headers of packfiles in new data folder
            for (var file in Directory.EnumerateFiles(_dataFolder))
            {
                if (file.GetExtension(.. scope .()) != ".vpp_pc")
                    continue;

                String filePath = file.GetFilePath(.. scope .());
                PackfileV3 packfile = new .(filePath);
                if (packfile.ReadMetadata() case .Err(let err))
                {
                    delete packfile;
                    Runtime.FatalError(scope $"Failed to read packfile. Error: '{err}'"); //TODO: Add proper error logging instead of crashing
                }
                if (packfile.ReadAsmFiles() case .Err(let err))
                {
                    delete packfile;
                    Runtime.FatalError(scope $"Failed to read asm_pc file in packfile. Error: '{err}'"); //TODO: Add proper error logging instead of crashing
                }
                Packfiles.Add(packfile);
            }
        }

        public static Result<PackfileV3, StringView> GetPackfile(StringView packfileName)
        {
            for (PackfileV3 packfile in Packfiles)
                if (StringView.Equals(packfile.Name, packfileName, true))
                    return packfile;

            return .Err("Failed to find packfile with the provided name");
        }

        //Extract a container (.str2_pc file) from a .vpp_pc file. The caller is responsible for deleting the container on success.
        private static Result<PackfileV3, StringView> GetContainer(StringView name, StringView parentName)
        {
            if (GetPackfile(parentName) case .Ok(PackfileV3 packfile))
            {
                switch (packfile.ReadSingleFile(name))
                {
                    case .Ok(u8[] containerBytes):
                        //TODO: Change this to avoid making a copy. Need a version of MemorySteam that can take u8[] instead of List<u8>
                        //      ... or make ReadSingleFile return List<u8>. Would rather not since that seems messier than using u8[]
                        PackfileV3 container = new .(new MemoryStream(containerBytes.ToList(.. new List<u8>()), owns: true), name);
                        delete containerBytes;
                        container.ReadMetadata();
                        return container;
                    case .Err(StringView err):
                        return .Err("Failed to extract container");
                }
            }
            else
            {
                Logger.Error("Packfile.GetContainer() failed to extract the parent packfile '{}'", parentName);
                return .Err("Failed to extract parent packfile");
            }
        }

        //Extract a file using it's path. E.g. "humans.vpp_pc/rfg_PC.str2_pc/mason_head.cpeg_pc"
        public static Result<u8[], StringView> GetFileBytes(StringView path)
        {
            //Split path by '/' into components
            StringView packfile = "", container = "", file = "";
            List<StringView> split = path.Split("/").ToList(.. scope List<StringView>());
            switch (split.Count)
            {
                case 2: //Packfile/file
                    packfile = split[0];
                    file = split[1];
                    break;
                case 3: //Packfile/container/file
                    packfile = split[0];
                    container = split[1];
                    file = split[2];
                    break;
                case 4: //Packfile/container/file/subfile
                    packfile = split[0];
                    container = split[1];
                    file = split[2];
                case 0:
                    return .Err("Path is empty. Can't parse.");
                case 1:
                    return .Err("Filename isn't present in path. Can't parse.");
                default:
                    return .Err("Failed to parse packfile path");
            }

            //Validate path components
            if (Path.GetExtension(packfile, .. scope .()) != ".vpp_pc")
            {
                Logger.Error("Incorrect packfile extension passed to PackfileVFS.GetFileBytes(). Expected .vpp_pc, found {}", Path.GetExtension(packfile, .. scope .()));
                return .Err("Incorrect packfile extension. See the log");
            }
            if (container != "" && Path.GetExtension(container, .. scope .()) != ".str2_pc")
            {
                Logger.Error("Incorrect container extension passed to PackfileVFS.GetFileBytes(). Expected .str2_pc, found {}", Path.GetExtension(packfile, .. scope .()));
                return .Err("Incorrect container extension. See the log");
            }
            bool hasContainer = (container != "");

            //Get packfile that contains target file
            PackfileV3 parent = hasContainer ? GetContainer(container, packfile) : GetPackfile(packfile);
            if (parent == null)
            {
                Logger.Error("Failed to extract parent file in PackfileVFS.GetFileBytes().");
                return .Err("Failed to extract parent file in PackfileVFS.GetFileBytes().");
            }
            defer { if (hasContainer) delete parent; }

            //Extract file
            switch (parent.ReadSingleFile(file))
            {
                case .Ok(u8[] bytes):
                    return bytes;
                case .Err(StringView err):
                    return .Err("Failed to extract target file");
            }
        }

        public static Result<void, StringView> GetFileString(StringView path, String result)
        {
            switch (GetFileBytes(path))
            {
                case .Ok(u8[] bytes):
                    defer delete bytes;
                    result.Set(StringView((char8*)bytes.CArray(), bytes.Count));
                    return .Ok;
                case .Err(StringView err):
                    return .Err(err);
            }
        }
	}
}