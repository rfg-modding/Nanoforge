#pragma once
#include <vector>

enum FileExplorerNodeType
{
    Packfile, //Top level packfile (.vpp_pc file)
    Container, //Internal packfile/container (.str2_pc file)
    Primitive, //Any file that's not a .vpp_pc or .str2_pc file
};
struct FileExplorerNode
{
    FileExplorerNode(string text, FileExplorerNodeType type, bool inContainer, string filename, string parentName) 
        : Text(text), Type(type), InContainer(inContainer), Filename(filename), ParentName(parentName)
    {
        Children = {};
        Selected = false;
    }

    string Text;
    std::vector<FileExplorerNode> Children = {};
    FileExplorerNodeType Type = Packfile;
    bool InContainer = false;
    bool Selected = false;
    //If true ::Text matches the most recent search term
    bool MatchesSearchTerm = true;
    //If true ::Text for at least one child node matches the most recent search term
    bool AnyChildNodeMatchesSearchTerm = true;

    //Data used to access the file when it's opened via the file explorer
    string Filename;
    string ParentName;

    //Whether or not the node is open. Set each frame while drawing nodes. Editing this doesn't change the nodes open state.
    bool Open = false;
};