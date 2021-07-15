#include <BinaryTools/BinaryReader.h>
#include "PackfileVFS.h"
#include "FileHandle.h"
#include "Log.h"

FileHandle::FileHandle(Packfile3* packfile, const string& fileName, const string& containerName)
{
    if (!packfile)
        THROW_EXCEPTION("Null packfile pointer passed to FileHandle constructor.");

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
        if (!containerBytes)
            THROW_EXCEPTION("Failed to extract container from packfile.");

        //Parse container and get file byte buffer
        Packfile3 container(containerBytes.value());
        container.ReadMetadata();

        auto fileBytes = container.ExtractSingleFile(fileName_, true);
        if (!fileBytes)
            THROW_EXCEPTION("Failed to extract file from container.");

        //Return file byte buffer
        return fileBytes.value();
    }
    else
    {
        //Find file and return it
        auto file = packfile_->ExtractSingleFile(fileName_, false);
        if(!file)
            THROW_EXCEPTION("Failed to extract file from packfile.");

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
    if (!containerBytes)
        THROW_EXCEPTION("Failed to extract container from packfile.");

    //Parse container and get file byte buffer
    Packfile3* container = new Packfile3(containerBytes.value());
    container->ReadMetadata();
    container->SetName(containerName_);

    return container;
}
