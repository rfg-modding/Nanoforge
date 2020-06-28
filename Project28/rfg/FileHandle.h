#pragma once
#include "common/Typedefs.h"
#include <span>

class PackfileVFS;
class Packfile3;

//Handle to file inside a packfile. Allows lazy loading of the full contents of files and avoids
//passing around large byte buffers of files.
class FileHandle
{
public:
    FileHandle(Packfile3* packfile, const string& fileName, const string& containerName = "");

    //Todo: Consider returning a class that auto-handles freeing the buffer here
    //Get the file as a byte array. The user must delete this buffer themselves or else it'll cause a memory leak
    std::span<u8> Get();
    //Get top level packfile that the file or it's container is stored in. User should not free this one
    Packfile3* GetPackfile();
    //Todo: Automate freeing / releasing this
    //Get container if the file is stored in one. User must free this pointer
    Packfile3* GetContainer();

    string Filename() { return fileName_; }
    string ContainerName() { return containerName_; }
    bool InContainer() { return fileInContainer_; }

private:
    Packfile3* packfile_ = nullptr;
    string fileName_;
    string containerName_;
    bool fileInContainer_ = false;
    
    PackfileVFS* vfs_ = nullptr;
};