#include "PackfileVFS.h"
#include "common/filesystem/Path.h"
#include "common/filesystem/File.h"
#include "common/string/String.h"
#include "application/project/Project.h"
#include <RfgTools++/formats/textures/PegFile10.h>
#include "Log.h"
#include <filesystem>
#include <iostream>
#include <ext/WindowsWrapper.h>

//Global file cache path. Extracted files are put in this folder to avoid repeat extractions
const string globalCachePath_ = ".\\Cache\\";

void PackfileVFS::Init(const string& packfileFolderPath, Project* project)
{
    TRACE();
    packfileFolderPath_ = packfileFolderPath; //RFG data folder path
    project_ = project;
    ready_ = false;
}

void PackfileVFS::ScanPackfilesAndLoadCache()
{
    TRACE();
    packfiles_.clear();

    //Loop through all files in data folder
    for (auto& filePath : std::filesystem::directory_iterator(packfileFolderPath_))
    {
        //Ignore non vpps
        if (Path::GetExtension(filePath) != ".vpp_pc")
            continue;

        //Load and parse the vpp, then push it to packfiles vector
        Packfile3& packfile = packfiles_.emplace_back(filePath.path().string());
        packfile.ReadMetadata();
        packfile.ReadAsmFiles();
    }
    ready_ = true;
}

std::vector<FileHandle> PackfileVFS::GetFiles(const std::vector<string>& searchFilters, bool recursive, bool oneResultPerFilter)
{
    //Vector for our file handles
    std::vector<FileHandle> handles = {};

    //Run search with each filter
    for (auto& currentFilter : searchFilters)
    {
        //By default just match the filename to the search string
        SearchType searchType = SearchType::Direct;
        s_view filter = currentFilter; //Adjusted search string

        //Search filtering options
        if (filter.front() == '*') //Ex: *.rfgzone_pc (Finds filenames that end with .rfgzone_pc)
        {
            searchType = SearchType::AnyStart;
            filter = filter.substr(1);
        }
        else if (filter.back() == '*') //Ex: always_loaded.* (Finds filenames with any extension that is named always_loaded)
        {
            searchType = SearchType::AnyEnd;
            filter = filter.substr(0, filter.size() - 2);
        }

        //Loop through packfiles
        for (auto& packfile : packfiles_)
        {
            //Todo: Add support for C&C packfiles
            //C&C files not supported since single file extraction isn't supported yet for them
            if (packfile.Compressed && packfile.Condensed)
                continue;

            //Loop through all files in the packfile
            for (u32 i = 0; i < packfile.Entries.size(); i++)
            {
                if (!CheckSearchMatch(packfile.EntryNames[i], filter, searchType))
                    continue;

                handles.emplace_back(&packfile, packfile.EntryNames[i]);
                //Break out to the top loop since we only want one result per filter
                if (oneResultPerFilter)
                    goto continueRoot;
            }

            //Stop here if we're not doing a recursive search
            if (!recursive)
                continue;

            //Do recursive search. Use asmFile data instead of opening each str2 to avoid unnecessary parsing
            for (auto& asmFile : packfile.AsmFiles)
            {
                for (auto& container : asmFile.Containers)
                {
                    for (auto& primitive : container.Primitives)
                    {
                        if (!CheckSearchMatch(primitive.Name, filter, searchType))
                            continue;

                        handles.emplace_back(&packfile, primitive.Name, container.Name + ".str2_pc");
                        //Break out to the top loop since we only want one result per filter
                        if (oneResultPerFilter)
                            goto continueRoot;
                    }
                }
            }
        }
    continueRoot:;
    }

    //Return handles to files which matched the search filters
    return handles;
}

std::vector<FileHandle> PackfileVFS::GetFiles(const string& packfileName, const string& filter, bool recursive, bool oneResultPerFilter)
{
    //Vector for our file handles
    std::vector<FileHandle> handles = {};

    //By default just match the filename to the search string
    SearchType searchType = SearchType::Direct;
    s_view filter0 = filter; //Adjusted search string

    //Search filtering options
    if (filter0.front() == '*') //Ex: *.rfgzone_pc (Finds filenames that end with .rfgzone_pc)
    {
        searchType = SearchType::AnyStart;
        filter0 = filter0.substr(1);
    }
    else if (filter0.back() == '*') //Ex: always_loaded.* (Finds filenames with any extension that is named always_loaded)
    {
        searchType = SearchType::AnyEnd;
        filter0 = filter0.substr(0, filter0.size() - 2);
    }

    //Get packfile
    Packfile3* packfile = GetPackfile(packfileName);
    if (!packfile || (packfile->Compressed && packfile->Condensed))
        return handles;

    //Loop through all files in the packfile
    for (u32 i = 0; i < packfile->Entries.size(); i++)
    {
        if (!CheckSearchMatch(packfile->EntryNames[i], filter0, searchType))
            continue;

        handles.emplace_back(packfile, packfile->EntryNames[i]);
        //Break out to the top loop since we only want one result per filter
        if (oneResultPerFilter)
            goto continueRoot;
    }

    //Stop here if we're not doing a recursive search
    if (recursive)
    {
        //Do recursive search. Use asmFile data instead of opening each str2 to avoid unnecessary parsing
        for (auto& asmFile : packfile->AsmFiles)
        {
            for (auto& container : asmFile.Containers)
            {
                for (auto& primitive : container.Primitives)
                {
                    if (!CheckSearchMatch(primitive.Name, filter0, searchType))
                        continue;

                    handles.emplace_back(packfile, primitive.Name, container.Name + ".str2_pc");
                    //Break out to the top loop since we only want one result per filter
                    if (oneResultPerFilter)
                        goto continueRoot;
                }
            }
        }
    continueRoot:;
    }

    //Return handles to files which matched the search filters
    return handles;
}

std::vector<FileHandle> PackfileVFS::GetFiles(const std::initializer_list<string>& searchFilters, bool recursive, bool findOne)
{
    return GetFiles(std::vector<string>(searchFilters), recursive, findOne);
}

std::vector<FileHandle> PackfileVFS::GetFiles(const string& filter, bool recursive, bool findOne)
{
    //Call primary overload which can take multiple filters
    return GetFiles(std::vector<string>{filter}, recursive, findOne);
}

Packfile3* PackfileVFS::GetPackfile(const string& name)
{
    for (auto& packfile : packfiles_)
    {
        if (packfile.Name() == name)
            return &packfile;
    }
    return nullptr;
}

Packfile3* PackfileVFS::GetContainer(const string& name, const string& parentName)
{
    Packfile3* packfile = GetPackfile(parentName);
    if (!packfile)
        THROW_EXCEPTION("Failed to get packfile '{}'", parentName);

    //Find container
    auto containerBytes = packfile->ExtractSingleFile(name, false);
    //Parse container and get file byte buffer
    Packfile3* container = new Packfile3(containerBytes.value());
    if (!container)
        THROW_EXCEPTION("Failed to get packfile '{}' from '{}'", name, parentName);

    container->ReadMetadata();
    container->SetName(name);
    return container;
}

bool PackfileVFS::CheckSearchMatch(s_view target, s_view filter, SearchType searchType)
{
    switch (searchType)
    {
    case SearchType::Direct:
        return filter == target;
    case SearchType::AnyStart:
        return String::EndsWith(target, filter);
    case SearchType::AnyEnd:
        return String::StartsWith(target, filter);
    default:
        THROW_EXCEPTION("Invalid or unsupported enum value \"{}\".", searchType);
    }
}