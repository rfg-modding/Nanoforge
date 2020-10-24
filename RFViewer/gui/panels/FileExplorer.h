#pragma once
#include "gui/GuiState.h"
#include "common/filesystem/Path.h"
#include "Log.h"

enum FileNodeType
{
    Packfile, //Top level packfile (.vpp_pc file)
    Container, //Internal packfile/container (.str2_pc file)
    Primitive, //A file that's inside a .str2_pc file
    Loose //Any file that's inside a .vpp_pc file which isn't a .str2_pc file. E.g. all the xtbls in misc.vpp_pc
};
struct FileNode
{
    FileNode(string text, FileNodeType type) : Text(text), Type(type)
    {
        Children = {};
        Selected = false;
    }

    string Text;
    std::vector<FileNode> Children = {};
    FileNodeType Type = Packfile;
    bool Selected = false;
};
std::vector<FileNode> FileTree = {};
FileNode* SelectedNode = nullptr;

//Generates file tree. Done once at startup and when files are added/removed (very rare)
//Pre-generating data like this makes rendering and interacting with the file tree simpler
void FileExplorer_GenerateFileTree(GuiState* state);
//Draws and imgui tree node for the provided FileNode
void FileExplorer_DrawFileNode(GuiState* state, FileNode& node);

void FileExplorer_Update(GuiState* state, bool* open)
{
    //Regen node tree if necessary
    if (state->FileTreeNeedsRegen)
        FileExplorer_GenerateFileTree(state);

    if (!ImGui::Begin("File explorer", open))
    {
        ImGui::End();
        return;
    }

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_ARCHIVE " Packfiles");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    //Draw nodes
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 0.75f); //Increase spacing to differentiate leaves from expanded contents.
    for (auto& node : FileTree)
    {
        FileExplorer_DrawFileNode(state, node);
    }
    ImGui::PopStyleVar();

    ImGui::End();
}

void FileExplorer_GenerateFileTree(GuiState* state)
{
    if (state->FileTreeLocked)
        return;

    //Clear current tree and selected node
    FileTree.clear();
    SelectedNode = nullptr;

    //Used to give unique names to each imgui tree node by appending "##index" to each name string
    u64 index = 0;
    //Loop through each top level packfile (.vpp_pc file)
    for (auto& packfile : state->PackfileVFS->packfiles_)
    {
        string packfileNodeLabel = packfile.Name() + " [" + std::to_string(packfile.Header.NumberOfSubfiles) + " subfiles";
        if (packfile.Compressed)
            packfileNodeLabel += ", Compressed";
        if (packfile.Condensed)
            packfileNodeLabel += ", Condensed";

        packfileNodeLabel += "]";

        FileNode& packfileNode = FileTree.emplace_back(packfileNodeLabel, Packfile);

        //Loop through each asm_pc file in the packfile
        //These are done separate from other files so str2_pc files and their asm_pc files are readily accessible. Important since game streams str2_pc files based on asm_pc contents
        for (auto& asmFile : packfile.AsmFiles)
        {
            //Loop through each container (.str2_pc file) represented by the asm_pc file
            for (auto& container : asmFile.Containers)
            {
                string containerName = container.Name + ".str2_pc" + "##" + std::to_string(index);
                FileNode& containerNode = packfileNode.Children.emplace_back(containerName, Container);
                for (auto& primitive : container.Primitives)
                {
                    string primitiveName = primitive.Name + "##" + std::to_string(index);
                    containerNode.Children.emplace_back(primitiveName, Primitive);
                    index++;
                }
                index++;
            }
        }

        //Loop through other contents of the vpp_pc that aren't in str2_pc files
        for (u32 i = 0; i < packfile.Entries.size(); i++)
        {
            const char* entryName = packfile.EntryNames[i];

            //Skip str2_pc files as we've already covered those
            if (Path::GetExtension(entryName) == ".str2_pc")
                continue;

            string looseFileName = string(entryName) + "##" + std::to_string(index);
            packfileNode.Children.emplace_back(looseFileName, Loose);
            index++;
        }
        index++;
    }

    state->FileTreeNeedsRegen = false;
}

void FileExplorer_DrawFileNode(GuiState* state, FileNode& node)
{
    //Draw node
    bool nodeOpen = ImGui::TreeNodeEx(node.Text.c_str(), 
                                     //Make full node width clickable
                                     ImGuiTreeNodeFlags_SpanAvailWidth
                                     //If the node has no children make it a leaf node (a node which can't be expanded)
                                     | (node.Children.size() == 0 ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None) 
                                     //Highlight the node if it's the currently selected node (the last node that was clicked)
                                     | (&node == SelectedNode ? ImGuiTreeNodeFlags_Selected : 0));

    //Check if the node was clicked. Set SelectedNode to most recently selected node
    if (ImGui::IsItemClicked())
        SelectedNode = &node;

    //If the node is open draw it's child nodes
    if (nodeOpen)
    {
        for (auto& childNode : node.Children)
        {
            FileExplorer_DrawFileNode(state, childNode);
        }
        ImGui::TreePop();
    }
}