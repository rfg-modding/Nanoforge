#include "FileCache.h"
#include "common/filesystem/Path.h"
#include "Common/filesystem/File.h"
#include "common/string/String.h"
#include <filesystem>

void FileCache::Load(const string& path)
{
    //Set path and root node
    cachePath_ = path;
    std::filesystem::create_directory(path);
    rootNode_ = FileNode("@root", "", FileNode::NodeType::Folder);

    //Recursively loop through files/folders to fill out FileNode tree
    rootNode_ = LoadNode(cachePath_, "@root");
}

bool FileCache::IsCached(const string& path)
{
    return rootNode_.IsCached(path);
}

void FileCache::AddFolder(const string& path)
{
    //Make sure parent folders exist
    std::filesystem::create_directories(cachePath_ + path);
    //Create node
    rootNode_.AddFolder(path);
}

void FileCache::AddFile(const string& path, std::span<u8> bytes)
{
    //Make sure parent folders exist
    std::filesystem::create_directories(cachePath_ + Path::GetParentDirectory(path));
    //Create node and save bytes to file path
    rootNode_.AddFile(path);
    File::WriteToFile(cachePath_ + path, bytes);
}

FileNode FileCache::LoadNode(const string& path, const string& name)
{
    //If node is a file return a file node
    bool isDirectory = std::filesystem::is_directory(path);
    if (!isDirectory)
        return FileNode(name, path, FileNode::NodeType::File);

    //If node is a folder then loop through subfiles/folders and create nodes for them by calling LoadNode again
    FileNode node(name, path, FileNode::NodeType::Folder);
    for (auto& entry : std::filesystem::directory_iterator(path))
        node.Subfiles.push_back(LoadNode(entry.path().string(), entry.path().filename().string()));

    return node;
}
