#include "FileExplorer.h"
#include "property_panel/PropertyPanelContent.h"
#include "gui/documents/TextureDocument.h"
#include "render/imgui/imgui_ext.h"
#include "common/string/String.h"

std::vector<FileNode> FileExplorer_FileTree = {};
FileNode* FileExplorer_SelectedNode = nullptr;

//Data used for file tree searches
//Buffer used by search text bar
string FileExplorer_SearchTerm = "";
//Search string that's passed directly to string comparison functions. Had * at start or end of search term removed
string FileExplorer_SearchTermPatched = "";
//If true, search term changed and need to recheck all nodes for search match
bool FileExplorer_SearchChanged = false;
enum FileSearchType
{
    Match,
    MatchStart,
    MatchEnd
};
FileSearchType FileExplorer_SearchType = Match;

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

    if (ImGui::InputText("Search", &FileExplorer_SearchTerm))
        FileExplorer_SearchChanged = true;

    ImGui::Separator();

    //Update search term
    if (FileExplorer_SearchTerm != "" && FileExplorer_SearchChanged)
    {
        if (FileExplorer_SearchTerm[0] == '*')
        {
            FileExplorer_SearchType = MatchEnd;
            FileExplorer_SearchTermPatched = FileExplorer_SearchTerm.substr(1);
        }
        else if (FileExplorer_SearchTerm.back() == '*')
        {
            FileExplorer_SearchType = MatchStart;
            FileExplorer_SearchTermPatched = FileExplorer_SearchTerm.substr(0, FileExplorer_SearchTerm.size() - 1);
        }
        else
        {
            FileExplorer_SearchType = Match;
            FileExplorer_SearchTermPatched = FileExplorer_SearchTerm;
        }
    }
    else if(FileExplorer_SearchChanged)
    {
        FileExplorer_SearchTermPatched = "";
    }

    //Draw nodes
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 0.75f); //Increase spacing to differentiate leaves from expanded contents.
    for (auto& node : FileExplorer_FileTree)
    {
        FileExplorer_VppName = node.Filename;
        FileExplorer_DrawFileNode(state, node);
    }
    ImGui::PopStyleVar();
    FileExplorer_SearchChanged = false;

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
        string packfileNodeText = packfile.Name();
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

bool DoesNodeFitSearch(FileNode& node)
{
    if (FileExplorer_SearchType == Match && !String::Contains(node.Text, FileExplorer_SearchTermPatched))
        return false;
    else if (FileExplorer_SearchType == MatchStart && !String::StartsWith(node.Text, FileExplorer_SearchTermPatched))
        return false;
    else if (FileExplorer_SearchType == MatchEnd)
    {
        //All non .vpp_pc FileNode names are postfixed with "##i", where i is they're node id. This an imgui label, used to avoid weirdness in case two files have the same name
        //We must strip that from the end of the string when doing MatchEnd searches so they're work properly
        size_t labelLocation = node.Text.find("##");
        string realName = node.Text;
        if (labelLocation != string::npos)
            realName = realName.substr(0, labelLocation);

        return String::EndsWith(realName, FileExplorer_SearchTermPatched);
    }
    else
        return true;
}

bool AnyChildNodesFitSearch(FileNode& node)
{
    for (auto& childNode : node.Children)
    {
        if (DoesNodeFitSearch(childNode))
            return true;
    }
    return false;
}

void UpdateNodeSearchResultsRecursive(FileNode& node)
{
    node.MatchesSearchTerm = DoesNodeFitSearch(node);
    node.AnyChildNodeMatchesSearchTerm = AnyChildNodesFitSearch(node);
    for (auto& childNode : node.Children)
        UpdateNodeSearchResultsRecursive(childNode);
}

void FileExplorer_DrawFileNode(GuiState* state, FileNode& node)
{
    static u32 maxDoubleClickTime = 500; //Max ms between 2 clicks on one item to count as a double click
    static Timer clickTimer; //Measures times between consecutive clicks on the same node. Used to determine if a double click occurred

    //If the search term changed then update cached search checks. They're cached to avoid checking the search term every frame
    if (FileExplorer_SearchChanged)
        UpdateNodeSearchResultsRecursive(node);
    //Make sure node fits search term
    if (!node.MatchesSearchTerm && !node.AnyChildNodeMatchesSearchTerm)
        return;

    //Draw node
    bool nodeOpen = ImGui::TreeNodeEx(node.Text.c_str(),
        //Make full node width clickable
        ImGuiTreeNodeFlags_SpanAvailWidth
        //If the node has no children make it a leaf node (a node which can't be expanded)
        | (node.Children.size() == 0 || !node.AnyChildNodeMatchesSearchTerm ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None)
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
        //Dont bother drawing child nodes if none of them match the search term
        if (!node.AnyChildNodeMatchesSearchTerm)
        {
            ImGui::TreePop();
            return;
        }

        //Draw child nodes if at least one matches search term
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