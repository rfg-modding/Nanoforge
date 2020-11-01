#pragma once
#include "Common/Typedefs.h"
#include "FileNode.h"
#include <vector>
#include <span>

//Stores and tracks files in a folder on the hard drive. Has functions to check if a file is in the cache and to open it
class FileCache
{
public:
    //Loads the cache from the provided path
    void Load(const string& path);

    //Checks if the target file is cached
    bool IsCached(const string& path);
    //Adds a folder with the provided path if not already present. If the folder is inside another folder that doesn't have a node then nodes will be created for both
    void AddFolder(const string& path);
    //Adds a file at the provided path if there's not already one there
    void AddFile(const string& path, std::span<u8> bytes);

private:
    //Load a node from the provided path. Will recursively load nodes within this node if it's a folder. Path is relative to cache root
    FileNode LoadNode(const string& path, const string& name);

    string cachePath_;
    FileNode rootNode_;
};