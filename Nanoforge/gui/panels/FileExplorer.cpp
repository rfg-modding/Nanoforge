#include "FileExplorer.h"
#include "property_panel/PropertyPanelContent.h"
#include "gui/documents/TextureDocument.h"
#include "gui/documents/StaticMeshDocument.h"
#include "gui/documents/XtblDocument.h"
#include "render/imgui/imgui_ext.h"
#include "common/string/String.h"
#include <regex>

std::vector<FileExplorerNode> FileExplorer_FileTree = {};
FileExplorerNode* FileExplorer_SelectedNode = nullptr;

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
bool FileExplorer_HideUnsupportedFormats = false;
bool FileExplorer_RegexSearch = false;
std::regex FileExplorer_Regex("");
bool FileExplorer_HadRegexError = false;
string FileExplorer_LastRegexError;

//Generates file tree. Done once at startup and when files are added/removed (very rare)
//Pre-generating data like this makes rendering and interacting with the file tree simpler
void FileExplorer_GenerateFileTree(GuiState* state);
//Draws and imgui tree node for the provided FileExplorerNode
void FileExplorer_DrawFileNode(GuiState* state, FileExplorerNode& node);
//Called when file is single clicked from the explorer. Used to update property panel content
void FileExplorer_SingleClickedFile(GuiState* state, FileExplorerNode& node);
//Called when a file is double clicked in the file explorer. Attempts to open a tool/viewer for the provided file
void FileExplorer_DoubleClickedFile(GuiState* state, FileExplorerNode& node);
//Returns true if extension is a format that is supported by Nanoforge. This includes partial support (viewing only)
bool FileExplorer_FormatSupported(const string& ext);
//Updates node search results. Stores result in FileExplorerNode::MatchesSearchTerm and FileExplorerNode::AnyChildNodeMatchesSearchTerm
void UpdateNodeSearchResultsRecursive(FileExplorerNode& node);

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

    //Todo: Put additional search options in popup menu or collapsing header to avoid clutter
    if (ImGui::CollapsingHeader("Options"))
    {
        if (ImGui::Checkbox("Hide unsupported formats", &FileExplorer_HideUnsupportedFormats))
            FileExplorer_SearchChanged = true;
        if (ImGui::Checkbox("Regex", &FileExplorer_RegexSearch))
            FileExplorer_SearchChanged = true;
    }
    if (ImGui::InputText("Search", &FileExplorer_SearchTerm))
        FileExplorer_SearchChanged = true;
    //Show error icon with regex error in tooltip if using regex search and it's invalid
    if (FileExplorer_RegexSearch && FileExplorer_HadRegexError)
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.79f, 0.0f, 0.0f, 1.0f));
        ImGui::Text(ICON_FA_EXCLAMATION_TRIANGLE);
        ImGui::PopStyleColor();
        gui::TooltipOnPrevious(FileExplorer_LastRegexError, state->FontManager->FontDefault.GetFont());
    }

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
    if (FileExplorer_SearchTerm != "" && FileExplorer_SearchChanged && FileExplorer_RegexSearch)
    {
        //Checks if the search string is a valid regex. This is terrible but it doesn't seem that C++ provides a better way of validating a regex
        try
        {
            std::regex tempRegex(FileExplorer_SearchTerm);
            FileExplorer_Regex = tempRegex;
            FileExplorer_HadRegexError = false;
        }
        catch (std::regex_error& ex)
        {
            FileExplorer_SearchChanged = false;
            FileExplorer_HadRegexError = true;
            FileExplorer_LastRegexError = ex.what();
        }
    }

    //If the search term changed then update cached search checks. They're cached to avoid checking the search term every frame
    if (FileExplorer_SearchChanged)
    {
        for (auto& node : FileExplorer_FileTree)
        {
            UpdateNodeSearchResultsRecursive(node);
        }
    }

    //Draw nodes
    if (ImGui::BeginChild("FileExplorerList"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 0.75f); //Increase spacing to differentiate leaves from expanded contents.
        for (auto& node : FileExplorer_FileTree)
        {
            FileExplorer_VppName = node.Filename;
            FileExplorer_DrawFileNode(state, node);
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }
    FileExplorer_SearchChanged = false;

    ImGui::End();
}

//Get icon for node based on node type and extension
string GetNodeIcon(const string& filename)
{
    string ext = Path::GetExtension(filename);
    if (ext == ".asm_pc")
        return ICON_FA_DATABASE " ";
    else if (ext == ".cpeg_pc" || ext == ".cvbm_pc" || ext == ".gpeg_pc" || ext == ".gvbm_pc")
        return ICON_FA_IMAGE " ";
    else if (ext == ".xtbl" || ext == ".mtbl" || ext == ".dtodx" || ext == ".gtodx" || ext == ".scriptx" || ext == ".vint_proj" || ext == ".lua")
        return ICON_FA_CODE " ";
    else if (ext == ".rfgzone_pc" || ext == ".layer_pc")
        return ICON_FA_MAP " ";
    else if (ext == ".csmesh_pc" || ext == ".gsmesh_pc" || ext == ".ccmesh_pc" || ext == ".gcmesh_pc")
        return ICON_FA_CUBE " ";
    else if (ext == ".ctmesh_pc" || ext == ".gtmesh_pc" || ext == ".cterrain_pc" || ext == ".gterrain_pc" || ext == ".cstch_pc" || ext == ".gstch_pc" || ext == ".cfmesh_pc")
        return ICON_FA_MOUNTAIN " ";
    else if (ext == ".rig_pc")
        return ICON_FA_MALE " ";
    else if (ext == ".rfglocatext" || ext == ".le_strings")
        return ICON_FA_LANGUAGE " ";
    else if (ext == ".fxo_kg")
        return ICON_FA_PAINT_BRUSH " ";
    else if (ext == ".ccar_pc" || ext == ".gcar_pc")
        return ICON_FA_CAR_SIDE " ";
    else if (ext == ".vint_doc")
        return ICON_FA_FILE_INVOICE " ";
    else if (ext == ".anim_pc")
        return ICON_FA_RUNNING " ";
    else if (ext == ".sim_pc")
        return ICON_FA_TSHIRT " ";
    else if (ext == ".cchk_pc" || ext == ".gchk_pc")
        return ICON_FA_HOME " ";
    else if (ext == ".fsmib")
        return ICON_FA_MAP_PIN " ";
    else if (ext == ".cefct_pc")
        return ICON_FA_FIRE " ";
    else if (ext == ".mat_pc")
        return ICON_FA_PALETTE " ";
    else if (ext == ".morph_pc")
        return ICON_FA_GRIN " ";
    else if (ext == ".xwb_pc" || ext == ".xsb_pc" || ext == ".xgs_pc" || ext == ".aud_pc" || ext == ".vfdvp_pc" || ext == ".rfgvp_pc")
        return ICON_FA_VOLUME_UP " ";
    else if (ext == ".vf3_pc")
        return ICON_FA_FONT " ";
    else
        return ICON_FA_FILE " ";
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
        string packfileNodeText = ICON_FA_ARCHIVE " " + packfile.Name();
        FileExplorerNode& packfileNode = FileExplorer_FileTree.emplace_back(packfileNodeText, Packfile, false, packfile.Name(), "");

        //Loop through each asm_pc file in the packfile
        //These are done separate from other files so str2_pc files and their asm_pc files are readily accessible. Important since game streams str2_pc files based on asm_pc contents
        for (auto& asmFile : packfile.AsmFiles)
        {
            //Loop through each container (.str2_pc file) represented by the asm_pc file
            for (auto& container : asmFile.Containers)
            {
                string containerNodeText = ICON_FA_ARCHIVE " " + container.Name + ".str2_pc" + "##" + std::to_string(index);
                FileExplorerNode& containerNode = packfileNode.Children.emplace_back(containerNodeText, Container, false, container.Name + ".str2_pc", packfile.Name());
                for (auto& primitive : container.Primitives)
                {
                    string primitiveNodeText = GetNodeIcon(primitive.Name) + primitive.Name + "##" + std::to_string(index);
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

            string looseFileNodeText = GetNodeIcon(entryName) + string(entryName) + "##" + std::to_string(index);
            packfileNode.Children.emplace_back(looseFileNodeText, Primitive, false, string(entryName), packfile.Name());
            index++;
        }
        index++;
    }

    //Sort nodes alphabetically
    //First sort vpp nodes
    std::sort(FileExplorer_FileTree.begin(), FileExplorer_FileTree.end(), [](FileExplorerNode& a, FileExplorerNode& b) -> bool { return a.Filename < b.Filename; });
    for (auto& packfileNode : FileExplorer_FileTree)
    {
        //Then sort vpp node contents
        std::sort(packfileNode.Children.begin(), packfileNode.Children.end(), [](FileExplorerNode& a, FileExplorerNode& b) -> bool { return a.Filename < b.Filename; });

        //Then sort str2 node contents
        for (auto& containerNode : packfileNode.Children)
        {
            std::sort(containerNode.Children.begin(), containerNode.Children.end(), [](FileExplorerNode& a, FileExplorerNode& b) -> bool { return a.Filename < b.Filename; });
        }
    }

    state->FileTreeNeedsRegen = false;
}

//Returns true if the node text matches the current search term
bool DoesNodeFitSearch(FileExplorerNode& node)
{
    //Hide unsupported formats
    if (FileExplorer_HideUnsupportedFormats && !FileExplorer_FormatSupported(Path::GetExtension(node.Filename)))
        return false;

    if (FileExplorer_RegexSearch)
        return std::regex_search(node.Filename, FileExplorer_Regex);

    //Wildcard support
    if (FileExplorer_SearchType == Match && !String::Contains(node.Filename, FileExplorer_SearchTermPatched))
        return false;
    else if (FileExplorer_SearchType == MatchStart && !String::StartsWith(node.Filename, FileExplorer_SearchTermPatched))
        return false;
    else if (FileExplorer_SearchType == MatchEnd && !String::EndsWith(node.Filename, FileExplorer_SearchTermPatched))
        return false;
    else
        return true;
}

//Returns true if any of the child nodes of node match the current search term
bool AnyChildNodesFitSearch(FileExplorerNode& node)
{
    for (auto& childNode : node.Children)
    {
        if (DoesNodeFitSearch(childNode) || AnyChildNodesFitSearch(childNode))
            return true;
    }
    return false;
}

//Updates node search results. Stores result in FileExplorerNode::MatchesSearchTerm and FileExplorerNode::AnyChildNodeMatchesSearchTerm
void UpdateNodeSearchResultsRecursive(FileExplorerNode& node)
{
    node.MatchesSearchTerm = DoesNodeFitSearch(node);
    node.AnyChildNodeMatchesSearchTerm = AnyChildNodesFitSearch(node);
    for (auto& childNode : node.Children)
        UpdateNodeSearchResultsRecursive(childNode);
}

void FileExplorer_DrawFileNode(GuiState* state, FileExplorerNode& node)
{
    static u32 maxDoubleClickTime = 500; //Max ms between 2 clicks on one item to count as a double click
    static Timer clickTimer; //Measures times between consecutive clicks on the same node. Used to determine if a double click occurred

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

void FileExplorer_SingleClickedFile(GuiState* state, FileExplorerNode& node)
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

void FileExplorer_DoubleClickedFile(GuiState* state, FileExplorerNode& node)
{
    string extension = Path::GetExtension(node.Filename);
    if (extension == ".cpeg_pc" || extension == ".cvbm_pc" || extension == ".gpeg_pc" || extension == ".gvbm_pc")
    {
        //Convert gpu filenames to cpu filename if necessary. TextureDocument expects a cpu file name as input
        string filename;
        if (extension == ".cpeg_pc" || extension == ".cvbm_pc")
            filename = node.Filename;
        else if (extension == ".gpeg_pc")
            filename = Path::GetFileNameNoExtension(node.Filename) + ".cpeg_pc";
        else if(extension == ".gvbm_pc")
            filename = Path::GetFileNameNoExtension(node.Filename) + ".cvbm_pc";

        state->CreateDocument(filename, &TextureDocument_Init, &TextureDocument_Update, &TextureDocument_OnClose, new TextureDocumentData
        {
            .Filename = filename,
            .ParentName = node.ParentName,
            .VppName = FileExplorer_VppName,
            .InContainer = node.InContainer
        });
    }
    else if (extension == ".csmesh_pc" || extension == ".gsmesh_pc" || extension == ".ccmesh_pc" || extension == ".gcmesh_pc")
    {
        //Convert gpu filenames to cpu filename if necessary. StaticMeshDocument expects a cpu file name as input
        string filename;
        if (extension == ".csmesh_pc")
            filename = node.Filename;
        else if (extension == ".gsmesh_pc")
            filename = Path::GetFileNameNoExtension(node.Filename) + ".csmesh_pc";
        else if (extension == ".ccmesh_pc")
            filename = node.Filename;
        else if (extension == ".gcmesh_pc")
            filename = Path::GetFileNameNoExtension(node.Filename) + ".ccmesh_pc";

        state->CreateDocument(filename, &StaticMeshDocument_Init, &StaticMeshDocument_Update, &StaticMeshDocument_OnClose, new StaticMeshDocumentData
        {
            .Filename = filename,
            .ParentName = node.ParentName,
            .VppName = FileExplorer_VppName,
            .InContainer = node.InContainer
        });
    }
    else if (extension == ".xtbl")
    {
        state->CreateDocument(node.Filename, &XtblDocument_Init, &XtblDocument_Update, &XtblDocument_OnClose, new XtblDocumentData
        {
            .Filename = node.Filename,
            .ParentName = node.ParentName,
            .VppName = FileExplorer_VppName,
            .InContainer = node.InContainer
        });
    }
}

bool FileExplorer_FormatSupported(const string& ext)
{
    const std::vector<string> supportedFormats = { /*".vpp_pc", ".str2_pc",*/ ".cpeg_pc", ".cvbm_pc", ".gpeg_pc", ".gvbm_pc", ".csmesh_pc", ".gsmesh_pc", ".ccmesh_pc", ".gcmesh_pc" };
    for (auto& format : supportedFormats)
    {
        if (ext == format)
            return true;
    }

    return false;
}