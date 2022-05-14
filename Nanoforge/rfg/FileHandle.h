#pragma once
#include "common/Typedefs.h"
#include "common/String.h"
#include "common/Handle.h"
#include <optional>
#include <vector>

class PackfileVFS;
class Packfile3;

//Pair of files (cpu file + gpu file). Many RFG formats split a file into these. Typically the gpu file is data that gets loaded straight into vram, like pixels or vertices.
struct FilePair
{
    std::vector<u8> CpuFile;
    std::vector<u8> GpuFile;
};

//Handle to file inside a packfile. Allows lazy loading of the full contents of files and avoids passing around large byte buffers of files.
class FileHandle
{
public:
    FileHandle(Handle<Packfile3> packfile, std::string_view fileName, std::string_view containerName = "");

    //Get the file as a byte array
    std::vector<u8> Get();
    //Get file and it's sibling if one exists. E.g. if the file is a .cpeg_pc it'll try to load that and the matching gpeg_pc. Returns nothing if loading one or both files fails.
    //The primary file must be a cpu file for this to work. The extension also must be supported by RfgUtil::CpuFilenameToGpuFilename() (see util/RfgUtil.cpp)
    std::optional<FilePair> GetPair();
    //Get top level packfile that the file or its container is stored in
    Handle<Packfile3> GetPackfile();
    //Get container the file is stored in if it is. Returns nullptr if the file isn't stored in a container
    Handle<Packfile3> GetContainer();

    string Filename() { return fileName_; }
    string Extension();
    string ContainerName() { return containerName_; }
    bool InContainer() { return fileInContainer_; }

private:
    Handle<Packfile3> packfile_ = nullptr;
    string fileName_;
    string containerName_;
    bool fileInContainer_ = false;

    PackfileVFS* vfs_ = nullptr;
};