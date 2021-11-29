#include "PackfileVFS.h"
#include "common/filesystem/Path.h"
#include "common/filesystem/File.h"
#include "common/string/String.h"
#include "application/project/Project.h"
#include <RfgTools++\formats\packfiles\Packfile3.h>
#include "Log.h"
#include <filesystem>

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
        Handle<Packfile3> packfile = CreateHandle<Packfile3>(filePath.path().string());
        packfile->ReadMetadata();
        packfile->ReadAsmFiles();
        packfiles_.push_back(packfile);
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
            filter = filter.substr(0, filter.size() - 1);
        }

        //Loop through packfiles
        for (auto& packfile : packfiles_)
        {
            //Loop through all files in the packfile
            for (u32 i = 0; i < packfile->Entries.size(); i++)
            {
                if (!CheckSearchMatch(packfile->EntryNames[i], filter, searchType))
                    continue;

                handles.emplace_back(packfile, packfile->EntryNames[i]);
                //Break out to the top loop since we only want one result per filter
                if (oneResultPerFilter)
                    goto continueRoot;
            }

            //Stop here if we're not doing a recursive search
            if (!recursive)
                continue;

            //Do recursive search. Use asmFile data instead of opening each str2 to avoid unnecessary parsing
            for (auto& asmFile : packfile->AsmFiles)
            {
                for (auto& container : asmFile.Containers)
                {
                    for (auto& primitive : container.Primitives)
                    {
                        if (!CheckSearchMatch(primitive.Name, filter, searchType))
                            continue;

                        handles.emplace_back(packfile, primitive.Name, container.Name + ".str2_pc");
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
    else if (filter0.back() == '*') //Ex: always_loaded.* (Finds filenames that start with always_loaded)
    {
        searchType = SearchType::AnyEnd;
        filter0 = filter0.substr(0, filter0.size() - 2);
    }

    //Get packfile
    Handle<Packfile3> packfile = GetPackfile(packfileName);
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

std::optional<std::vector<u8>> PackfileVFS::GetFileBytes(const string& filePath)
{
    //Split path by '/' into components
    std::string_view packfile = "", container = "", file = "";
    std::vector<std::string_view> split = String::SplitString(filePath, "/");
    switch (split.size())
    {
    case 2: //Packfile/file
        packfile = split[0];
        file = split[1];
        break;
    case 3: //Packfile/container/file
        packfile = split[0];
        container = split[1];
        file = split[2];
        break;
    case 4: //Packfile/container/file/subfile
        packfile = split[0];
        container = split[1];
        file = split[2];
        break;
    case 0: //Empty path
        LOG_ERROR("Empty path passed to PackfileVFS::GetFileBytes(). Can't parse.");
    case 1: //Packfile/
        LOG_ERROR("Invalid path passed to PackfileVFS::GetFileBytes(). Filename isn't present, can't parse.");
    default:
        return {};
        break;
    }

    //Validate path components
    if (Path::GetExtension(packfile) != ".vpp_pc")
    {
        LOG_ERROR("Invalid path passed to PackfileVFS::GetFileBytes(). Packfile extension is wrong. Expected '.vpp_pc', found {}", Path::GetExtension(packfile));
        return {};
    }
    if (container != "" && Path::GetExtension(container) != ".str2_pc")
    {
        LOG_ERROR("Invalid path passed to PackfileVFS::GetFileBytes(). Container extension is wrong. Expected '.str2_pc', found {}", Path::GetExtension(container));
        return {};
    }
    bool hasContainer = (container != "");

    //Path of the file if its in the project cache
    string editedFilePath = project_->GetCachePath() + string(packfile) + "\\";
    if (container != "") editedFilePath += string(container) + "\\";
    editedFilePath += string(file);

    //Find and load file
    if (project_ && std::filesystem::exists(editedFilePath)) //Check project cache first
    {
        return File::ReadAllBytes(editedFilePath);
    }
    else //Otherwise load the file from packfiles
    {
        //Get packfile that contains target file
        Handle<Packfile3> parent = hasContainer ? GetContainer(container, packfile) : GetPackfile(packfile);
        if (!parent)
        {
            LOG_ERROR("Failed to extract parent file in PackfileVFS::GetFileBytes().");
            return {};
        }

        //Extract file
        std::optional<std::vector<u8>> bytes = parent->ExtractSingleFile(file, true);
        if (!bytes)
        {
            LOG_ERROR("Failed to extract target file '{}' in PackfileVFS::GetFileBytes()", file);
            return {};
        }

        return bytes;
    }
}

std::optional<string> PackfileVFS::GetFileString(const string& filePath)
{
    //Read file
    std::optional<std::vector<u8>> fileBytes = GetFileBytes(filePath);
    if (!fileBytes)
        return {};

    //Copy file into a string
    string out;
    out.resize(fileBytes.value().size());
    memcpy(out.data(), fileBytes.value().data(), fileBytes.value().size());
    return out;
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

Handle<Packfile3> PackfileVFS::GetPackfile(const std::string_view name)
{
    for (auto& packfile : packfiles_)
        if (packfile->Name() == name)
            return packfile;

    return nullptr;
}

Handle<Packfile3> PackfileVFS::GetContainer(const std::string_view name, const std::string_view parentName)
{
    Handle<Packfile3> packfile = GetPackfile(parentName);
    if (!packfile)
        THROW_EXCEPTION("Failed to get packfile '{}'", parentName);

    //Load container
    std::optional<std::vector<u8>> containerBytes = packfile->ExtractSingleFile(name, false);
    if (!containerBytes)
        THROW_EXCEPTION("Failed to get packfile '{}' from '{}'", name, parentName);

    //Parse container
    Handle<Packfile3> container = CreateHandle<Packfile3>(containerBytes.value());
    container->ReadMetadata();
    container->SetName(string(name));
    return container;
}

bool PackfileVFS::CheckSearchMatch(s_view target, s_view filter, SearchType searchType)
{
    switch (searchType)
    {
    case SearchType::Direct:
        return String::EqualIgnoreCase(filter, target);
    case SearchType::AnyStart:
        return String::EndsWith(String::ToLower(target), String::ToLower(filter));
    case SearchType::AnyEnd:
        return String::StartsWith(String::ToLower(target), String::ToLower(filter));
    default:
        THROW_EXCEPTION("Invalid or unsupported enum value \"{}\".", searchType);
    }
}