#pragma once
#include <vector>

enum FileNodeType
{
    Packfile, //Top level packfile (.vpp_pc file)
    Container, //Internal packfile/container (.str2_pc file)
    Primitive, //A file that's inside a .str2_pc file
    Loose //Any file that's inside a .vpp_pc file which isn't a .str2_pc file. E.g. all the xtbls in misc.vpp_pc
};
struct FileNode
{
    FileNode(string text, FileNodeType type, string filename, string parentName) : Text(text), Type(type), Filename(filename), ParentName(parentName)
    {
        Children = {};
        Selected = false;
    }

    string Text;
    std::vector<FileNode> Children = {};
    FileNodeType Type = Packfile;
    bool Selected = false;

    //Data used to access the file when it's opened via the file explorer
    string Filename;
    string ParentName;
};