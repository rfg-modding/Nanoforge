#pragma once
#include "common/Typedefs.h"
#include <vector>

class PackfileVFS;
class Packfile3;

//Handle to file inside a packfile. Allows lazy loading of the full contents of files and avoids passing around large byte buffers of files.
class FileHandle
{
public:
    FileHandle(Handle<Packfile3> packfile, std::string_view fileName, std::string_view containerName = "");

    //Get the file as a byte array
    std::vector<u8> Get();
    //Get top level packfile that the file or its container is stored in
    Handle<Packfile3> GetPackfile();
    //Get container the file is stored in if it is. Returns nullptr if the file isn't stored in a container
    Handle<Packfile3> GetContainer();

    string Filename() { return fileName_; }
    string ContainerName() { return containerName_; }
    bool InContainer() { return fileInContainer_; }

private:
    Handle<Packfile3> packfile_ = nullptr;
    string fileName_;
    string containerName_;
    bool fileInContainer_ = false;

    PackfileVFS* vfs_ = nullptr;
};