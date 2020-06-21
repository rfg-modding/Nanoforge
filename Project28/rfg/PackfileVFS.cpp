#include "PackfileVFS.h"
#include "common/filesystem/Path.h"
#include <filesystem>
#include <iostream>

PackfileVFS::~PackfileVFS()
{
    for (auto& packfile : packfiles_)
        packfile.Cleanup();
}

void PackfileVFS::ScanPackfiles()
{
    for (auto& filePath : std::filesystem::directory_iterator(packfileFolderPath))
    {
        if (Path::GetExtension(filePath) == ".vpp_pc")
        {
            try 
            {
                Packfile3 packfile(filePath.path().string());
                packfile.ReadMetadata();
                packfile.ReadAsmFiles();
                packfiles_.push_back(packfile);
            }
            catch (std::exception& ex)
            {
                std::cout << "Failed to parse vpp_pc file at \"" << filePath << "\". Error message: \"" << ex.what() << "\"\n";
            }
        }
    }
}
