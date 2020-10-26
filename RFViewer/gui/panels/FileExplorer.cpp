#include "FileExplorer.h"
#include "property_panel/PropertyPanelContent.h"
#include "gui/documents/TextureDocument.h"

std::vector<FileNode> FileExplorer_FileTree = {};
FileNode* FileExplorer_SelectedNode = nullptr;

//Generates file tree. Done once at startup and when files are added/removed (very rare)
//Pre-generating data like this makes rendering and interacting with the file tree simpler
void FileExplorer_GenerateFileTree(GuiState* state);
//Draws and imgui tree node for the provided FileNode
void FileExplorer_DrawFileNode(GuiState* state, FileNode& node);
//Called when file is single clicked from the explorer. Used to update property panel content
void FileExplorer_SingleClickedFile(GuiState* state, FileNode& node);
//Called when a file is double clicked in the file explorer. Attempts to open a tool/viewer for the provided file
void FileExplorer_DoubleClickedFile(GuiState* state, FileNode& node);

string FileExplorer_VppName;
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
    for (auto& node : FileExplorer_FileTree)
    {
        FileExplorer_VppName = node.Filename;
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
    FileExplorer_FileTree.clear();
    FileExplorer_SelectedNode = nullptr;

    //Used to give unique names to each imgui tree node by appending "##index" to each name string
    u64 index = 0;
    //Loop through each top level packfile (.vpp_pc file)
    for (auto& packfile : state->PackfileVFS->packfiles_)
    {
        string packfileNodeText = packfile.Name() + " [" + std::to_string(packfile.Header.NumberOfSubfiles) + " subfiles";
        if (packfile.Compressed)
            packfileNodeText += ", Compressed";
        if (packfile.Condensed)
            packfileNodeText += ", Condensed";

        packfileNodeText += "]";

        FileNode& packfileNode = FileExplorer_FileTree.emplace_back(packfileNodeText, Packfile, false, packfile.Name(), "");

        //Loop through each asm_pc file in the packfile
        //These are done separate from other files so str2_pc files and their asm_pc files are readily accessible. Important since game streams str2_pc files based on asm_pc contents
        for (auto& asmFile : packfile.AsmFiles)
        {
            //Loop through each container (.str2_pc file) represented by the asm_pc file
            for (auto& container : asmFile.Containers)
            {
                string containerNodeText = container.Name + ".str2_pc" + "##" + std::to_string(index);
                FileNode& containerNode = packfileNode.Children.emplace_back(containerNodeText, Container, false, container.Name, packfile.Name());
                for (auto& primitive : container.Primitives)
                {
                    string primitiveNodeText = primitive.Name + "##" + std::to_string(index);
                    containerNode.Children.emplace_back(primitiveNodeText, Primitive, true, primitive.Name, container.Name + ".str2_pc");
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

            string looseFileNodeText = string(entryName) + "##" + std::to_string(index);
            packfileNode.Children.emplace_back(looseFileNodeText, Primitive, false, string(entryName), packfile.Name());
            index++;
        }
        index++;
    }

    state->FileTreeNeedsRegen = false;
}

void FileExplorer_DrawFileNode(GuiState* state, FileNode& node)
{
    static u32 maxDoubleClickTime = 500; //Max ms between 2 clicks on one item to count as a double click
    static Timer clickTimer; //Measures times between consecutive clicks on the same node. Used to determine if a double click occurred

    //Draw node
    bool nodeOpen = ImGui::TreeNodeEx(node.Text.c_str(),
        //Make full node width clickable
        ImGuiTreeNodeFlags_SpanAvailWidth
        //If the node has no children make it a leaf node (a node which can't be expanded)
        | (node.Children.size() == 0 ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None)
        //Highlight the node if it's the currently selected node (the last node that was clicked)
        | (&node == FileExplorer_SelectedNode ? ImGuiTreeNodeFlags_Selected : 0));

    //Check if the node was clicked and detect double clicks
    if (ImGui::IsItemClicked())
    {
        //Behavior for double clicking a file
        if (FileExplorer_SelectedNode == &node)
        {
            //If time between clicks less than maxDoubleClickTime then a double click occurred
            if (clickTimer.ElapsedMilliseconds() < maxDoubleClickTime)
                FileExplorer_DoubleClickedFile(state, node);

            clickTimer.Reset();
        }

        //Set FileExplorer_SelectedNode to most recently selected node
        FileExplorer_SelectedNode = &node;
        FileExplorer_SingleClickedFile(state, node);
        clickTimer.Reset();
    }
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

void FileExplorer_SingleClickedFile(GuiState* state, FileNode& node)
{
    string extension = Path::GetExtension(node.Filename);
    if (node.Type == Packfile)
    {
        state->PropertyPanelContentFuncPtr = &PropertyPanel_VppContent;
    }
    else if (node.Type == Container)
    {
        state->PropertyPanelContentFuncPtr = &PropertyPanel_Str2Content;
    }
    else if (node.Type == Primitive)
    {
        state->PropertyPanelContentFuncPtr = nullptr;
    }
    else
    {
        state->PropertyPanelContentFuncPtr = nullptr;
    }
}

void FileExplorer_DoubleClickedFile(GuiState* state, FileNode& node)
{
    string extension = Path::GetExtension(node.Filename);
    if (extension == ".cpeg_pc" || extension == ".cvbm_pc")
    {
        state->CreateDocument(Document(node.Filename, &TextureDocument_Init, &TextureDocument_Update, &TextureDocument_OnClose, new TextureDocumentData
        {
            .Filename = node.Filename,
            .ParentName = node.ParentName,
            .VppName = FileExplorer_VppName,
            .InContainer = node.InContainer
        }));
    }
}