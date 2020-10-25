#include "PackfileVFS.h"
#include "common/filesystem/Path.h"
#include "common/string/String.h"
#include "Log.h"
#include <filesystem>
#include <iostream>

void PackfileVFS::Init(const string& packfileFolderPath)
{
    packfileFolderPath_ = packfileFolderPath;
}

PackfileVFS::~PackfileVFS()
{
    for (auto& packfile : packfiles_)
        packfile.Cleanup();
}

void PackfileVFS::ScanPackfiles()
{
    for (auto& filePath : std::filesystem::directory_iterator(packfileFolderPath_))
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
            if (packfile.Compressed && packfile.Condensed)
                continue;

            for (u32 i = 0; i < packfile.Entries.size(); i++)
            {
                if (CheckSearchMatch(packfile.EntryNames[i], filter, searchType))
                {
                    handles.emplace_back(&packfile, packfile.EntryNames[i]);
                    //Break out to the top loop since we only want one result per filter
                    if (oneResultPerFilter)
                        goto continueRoot;
                }
            }
            if (recursive)
            {
                for (auto& asmFile : packfile.AsmFiles)
                {
                    for (auto& container : asmFile.Containers)
                    {
                        for (auto& primitive : container.Primitives)
                        {
                            if (CheckSearchMatch(primitive.Name, filter, searchType))
                            {
                                handles.emplace_back(&packfile, primitive.Name, container.Name + ".str2_pc");
                                //Break out to the top loop since we only want one result per filter
                                if (oneResultPerFilter)
                                    goto continueRoot;
                            }
                        }
                    }
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
        return nullptr;

    //Find container
    auto containerBytes = packfile->ExtractSingleFile(name, false);
    //Parse container and get file byte buffer
    Packfile3* container = new Packfile3(containerBytes.value());
    if (!container)
        return nullptr;

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
        THROW_EXCEPTION("Error! Invalid enum value passed to PackfileVFS::CheckSearchMatch");
    }
}
