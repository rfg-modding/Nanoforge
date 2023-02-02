using System.Collections;
using RfgTools.Formats;
using Common;
using System.IO;
using System;

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
	}
}