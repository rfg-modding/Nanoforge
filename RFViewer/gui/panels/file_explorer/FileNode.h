#pragma once
#include <vector>

enum FileNodeType
{
    Packfile, //Top level packfile (.vpp_pc file)
    Container, //Internal packfile/container (.str2_pc file)
    Primitive, //Any file that's not a .vpp_pc or .str2_pc file
};
struct FileNode
{
    FileNode(string text, FileNodeType type, bool inContainer, string filename, string parentName) : Text(text), Type(type), InContainer(inContainer), Filename(filename), ParentName(parentName)
    {
        Children = {};
        Selected = false;
    }

    string Text;
    std::vector<FileNode> Children = {};
    FileNodeType Type = Packfile;
    bool InContainer = false;
    bool Selected = false;

    //Data used to access the file when it's opened via the file explorer
    string Filename;
    string ParentName;
};