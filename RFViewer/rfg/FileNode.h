#pragma once
#include "common/Typedefs.h"
#include <vector>
#include <span>

//Represents a file or folder in a FileCache
struct FileNode
{
    //Whether the node is a file or folder
    enum NodeType
    {
        None,
        File,
        Folder,
    };
    FileNode() {}
    FileNode(const string& name, const string& internalPath, NodeType type) : Name(name), InternalPath(internalPath), Type(type) {}

    bool IsCached(const string& path);
    bool ContainsFolder(const string& path);
    FileNode& GetFolder(const string& path);
    FileNode& AddFolder(const string& path);
    FileNode& AddFile(const string& path);

    //The name of the file/folder that the node represents
    string Name;
    //The files path relative to the root of the cache it resides in
    string InternalPath;
    //Files that are contained by this file
    std::vector<FileNode> Subfiles;
    //Node type
    NodeType Type;
};