#include "PackfileVFS.h"
#include "common/filesystem/Path.h"
#include <filesystem>

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
            Packfile3 packfile(filePath.path().string());
            packfile.ReadMetadata();
            packfiles_.push_back(packfile);
        }
    }
}
