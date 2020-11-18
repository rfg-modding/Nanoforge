#include "FileNode.h"
#include "Common/string/String.h"
#include "Common/filesystem/Path.h"
#include "Log.h"

bool FileNode::IsCached(const string& path)
{
    std::vector<s_view> split = String::SplitString(path, "\\");
    for (auto& subfile : Subfiles)
    {
        if (String::EqualIgnoreCase(subfile.Name, split[0]))
            return split.size() == 1 ? true : subfile.IsCached(path.substr(path.find_first_of("\\") + 1));
    }
    return false;
}

bool FileNode::ContainsFolder(const string& path)
{
    std::vector<s_view> split = String::SplitString(path, "\\");
    for (auto& subfile : Subfiles)
    {
        if (String::EqualIgnoreCase(subfile.Name, split[0]))
            return split.size() == 1 ? true : subfile.ContainsFolder(path.substr(path.find_first_of("\\") + 1));
    }
    return false;
}

FileNode& FileNode::GetFolder(const string& path)
{
    std::vector<s_view> split = String::SplitString(path, "\\");
    for (auto& subfile : Subfiles)
    {
        if (String::EqualIgnoreCase(subfile.Name, split[0]))
            return split.size() == 1 ? subfile : subfile.GetFolder(path.substr(path.find_first_of("\\") + 1));
    }
    THROW_EXCEPTION("Failed to find folder \"{}\" in FileNode::GetFolder()", path);
}

//Todo: Double check usage of string_view or completely remove it
FileNode& FileNode::AddFolder(const string& path)
{
    //No duplicates allowed
    if (ContainsFolder(path))
        return GetFolder(path);

    std::vector<s_view> split = String::SplitString(path, "\\");
    if(split.size() > 1)
    {
        //Add root folder
        if (!ContainsFolder(string(split[0])))
            Subfiles.push_back(FileNode(Path::GetFileName(path), path, Folder));

        for (auto& subfile : Subfiles)
        {
            if (String::EqualIgnoreCase(subfile.Name, split[0]))
                return subfile.AddFolder(path.substr(path.find_first_of("\\") + 1));
        }
    }
    Subfiles.push_back(FileNode(Path::GetFileName(path), path, Folder));
    return Subfiles.back();
}

FileNode& FileNode::AddFile(const string& path)
{
    std::vector<s_view> split = String::SplitString(path, "\\");
    if (split.size() > 1)
    {
        FileNode& folder = AddFolder(path.substr(0, path.find_last_of("\\") + 1));
        folder.Subfiles.push_back(FileNode(Path::GetFileName(path), path, File));
        return folder.Subfiles.back();
    }
    Subfiles.push_back(FileNode(Path::GetFileName(path), path, File));
    return Subfiles.back();
}