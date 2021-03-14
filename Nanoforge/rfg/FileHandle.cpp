#include "FileHandle.h"
#include "PackfileVFS.h"
#include <BinaryTools/BinaryReader.h>

FileHandle::FileHandle(Packfile3* packfile, const string& fileName, const string& containerName)
{
#if DEBUG_BUILD
    if (!packfile)
        throw std::exception("Error! Attempted to create a file handle using a nullptr for a packfile.");
#endif

    packfile_ = packfile;
    fileName_ = fileName;
    containerName_ = containerName;
    fileInContainer_ = (containerName_ != "");
}

std::span<u8> FileHandle::Get()
{
    if (fileInContainer_)
    {
        //Find container
        auto containerBytes = packfile_->ExtractSingleFile(containerName_, false);
#if DEBUG_BUILD
        if (!containerBytes)
            throw std::exception("Error! Failed to extract container from packfile via file handle.");
#endif

        //Parse container and get file byte buffer
        Packfile3 container(containerBytes.value());
        container.ReadMetadata();

        auto fileBytes = container.ExtractSingleFile(fileName_, true);
#if DEBUG_BUILD
        if (!fileBytes)
            throw std::exception("Error! Failed to extract file from container via file handle.");
#endif

        //Return file byte buffer
        return fileBytes.value();
    }
    else
    {
        //Find file and return it
        auto file = packfile_->ExtractSingleFile(fileName_, false);
#if DEBUG_BUILD
        if(!file)
            throw std::exception("Error! Failed to extract single file from packfile via file handle.");
#endif
        return file.value();
    }
}

Packfile3* FileHandle::GetPackfile()
{
    return packfile_;
}

Packfile3* FileHandle::GetContainer()
{
    //Find container
    auto containerBytes = packfile_->ExtractSingleFile(containerName_, false);
#if DEBUG_BUILD
    if (!containerBytes)
        throw std::exception("Error! Failed to extract container from packfile via file handle.");
#endif

    //Parse container and get file byte buffer
    Packfile3* container = new Packfile3(containerBytes.value());
    container->ReadMetadata();
    container->SetName(containerName_);

    return container;
}
