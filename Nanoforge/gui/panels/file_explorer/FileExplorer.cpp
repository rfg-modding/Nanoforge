#include "FileExplorer.h"
#include "gui/panels/property_panel/PropertyPanelContent.h"
#include "gui/documents/TextureDocument.h"
#include "gui/documents/StaticMeshDocument.h"
#include "gui/documents/XtblDocument.h"
#include "gui/documents/AsmDocument.h"
#include "gui/documents/LocalizationDocument.h"
#include "render/imgui/imgui_ext.h"
#include "common/string/String.h"
#include "util/Profiler.h"
#include <regex>
#include <thread>

FileExplorer::FileExplorer()
{

}

FileExplorer::~FileExplorer()
{

}

void FileExplorer::Update(GuiState* state, bool* open)
{
    PROFILER_FUNCTION();
    if (!ImGui::Begin("File explorer", open))
    {
        ImGui::End();
        return;
    }

    //Don't update or draw file tree until the worker thread has finished reading packfile metadata
    if (!state->PackfileVFS->Ready())
    {
        FileTreeNeedsRegen = true;
        ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Loading packfiles...");
        ImGui::End();
        return;
    }

    //Regen node tree if necessary
    if (FileTreeNeedsRegen)
        GenerateFileTree(state);

    //Options
    if (ImGui::CollapsingHeader("Options"))
    {
        if (ImGui::Checkbox("Hide unsupported formats", &HideUnsupportedFormats))
            SearchChanged = true;
        if (ImGui::Checkbox("Regex", &RegexSearch))
            SearchChanged = true;
        if (ImGui::Checkbox("Case sensitive", &CaseSensitive))
            SearchChanged = true;
    }

    //Search bar
    UpdateSearchBar(state);
    if (searchTask_->Running())
    {
        ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Running search...");
        ImGui::End();
        return;
    }

    //Draw nodes
    if (ImGui::BeginChild("FileExplorerList"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 1.40f); //Increase spacing to differentiate leaves from expanded contents.
        for (auto& node : FileTree)
        {
            VppName = node.Filename;
            DrawFileNode(state, node);
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }

    ImGui::End();
}

void FileExplorer::UpdateSearchBar(GuiState* state)
{
    PROFILER_FUNCTION();
    if (ImGui::InputText("Search", &SearchTerm))
    {
        SearchChanged = true;
        searchChangeTimer_.Reset();
    }
    //Show error icon with regex error in tooltip if using regex search and it's invalid
    if (RegexSearch && HadRegexError)
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.79f, 0.0f, 0.0f, 1.0f));
        ImGui::Text(ICON_FA_EXCLAMATION_TRIANGLE);
        ImGui::PopStyleColor();
        gui::TooltipOnPrevious(LastRegexError, state->FontManager->FontDefault.GetFont());
    }
    ImGui::Separator();

    //Check if new search thread should be started. Will stop existing search thread if it's running
    bool startNewSearchThread = SearchChanged && searchChangeTimer_.ElapsedMilliseconds() > 500;
    if (!startNewSearchThread)
        return;

    //If existing search is running force it to stop
    if (searchTask_->Running())
    {
        searchTask_->CancelAndWait();
    }

    //If the search term changed then update cached search checks. They're cached to avoid checking the search term every frame
    std::scoped_lock(searchThreadMutex_);

    //Update search term
    if (SearchTerm != "" && SearchChanged)
    {
        if (SearchTerm[0] == '*')
        {
            SearchType = MatchEnd;
            SearchTermPatched = SearchTerm.substr(1);
        }
        else if (SearchTerm.back() == '*')
        {
            SearchType = MatchStart;
            SearchTermPatched = SearchTerm.substr(0, SearchTerm.size() - 1);
        }
        else
        {
            SearchType = Match;
            SearchTermPatched = SearchTerm;
        }
    }
    else if (SearchChanged)
    {
        SearchTermPatched = "";
    }

    //Check for regex errors
    if (SearchTerm != "" && SearchChanged && RegexSearch)
    {
        //Checks if the search string is a valid regex. This is terrible but it doesn't seem that C++ provides a better way of validating a regex
        try
        {
            std::regex tempRegex(SearchTerm);
            SearchRegex = tempRegex;
            HadRegexError = false;
            SearchTermPatched = SearchTerm;
        }
        catch (std::regex_error& ex)
        {
            SearchChanged = false;
            HadRegexError = true;
            LastRegexError = ex.what();
        }
    }

    //Start search thread
    SearchChanged = false;
    TaskScheduler::QueueTask(searchTask_, std::bind(&FileExplorer::SearchThread, this));
}

void FileExplorer::SearchThread()
{
    for (auto& node : FileTree)
    {
        //Exit early if search thread signalled to stop
        if (searchTask_->Cancelled())
            return;

        //Check if subnodes match search term
        UpdateNodeSearchResultsRecursive(node);
    }
}

void FileExplorer::GenerateFileTree(GuiState* state)
{
    //Clear current tree and selected node
    FileTree.clear();
    state->FileExplorer_SelectedNode = nullptr;

    //Empty space for node icons
    const string nodeIconSpacing = "      ";
    //Used to give unique names to each imgui tree node by appending "##index" to each name string
    u64 index = 0;
    //Loop through each top level packfile (.vpp_pc file)
    for (auto& packfile : state->PackfileVFS->packfiles_)
    {
        string packfileNodeText = nodeIconSpacing + packfile.Name();
        FileExplorerNode& packfileNode = FileTree.emplace_back(packfileNodeText, Packfile, false, packfile.Name(), "");

        //Loop through each asm_pc file in the packfile
        //These are done separate from other files so str2_pc files and their asm_pc files are readily accessible. Important since game streams str2_pc files based on asm_pc contents
        for (auto& asmFile : packfile.AsmFiles)
        {
            //Loop through each container (.str2_pc file) represented by the asm_pc file
            for (auto& container : asmFile.Containers)
            {
                string containerNodeText = nodeIconSpacing + container.Name + ".str2_pc" + "##" + std::to_string(index);
                FileExplorerNode& containerNode = packfileNode.Children.emplace_back(containerNodeText, Container, false, container.Name + ".str2_pc", packfile.Name());
                for (auto& primitive : container.Primitives)
                {
                    string primitiveNodeText = nodeIconSpacing + primitive.Name + "##" + std::to_string(index);
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

            string looseFileNodeText = nodeIconSpacing + string(entryName) + "##" + std::to_string(index);
            packfileNode.Children.emplace_back(looseFileNodeText, Primitive, false, string(entryName), packfile.Name());
            index++;
        }
        index++;
    }

    //Sort nodes alphabetically
    //First sort vpp nodes
    std::sort(FileTree.begin(), FileTree.end(), [](FileExplorerNode& a, FileExplorerNode& b) -> bool { return a.Filename < b.Filename; });
    for (auto& packfileNode : FileTree)
    {
        //Then sort vpp node contents
        std::sort(packfileNode.Children.begin(), packfileNode.Children.end(), [](FileExplorerNode& a, FileExplorerNode& b) -> bool { return a.Filename < b.Filename; });

        //Then sort str2 node contents
        for (auto& containerNode : packfileNode.Children)
        {
            std::sort(containerNode.Children.begin(), containerNode.Children.end(), [](FileExplorerNode& a, FileExplorerNode& b) -> bool { return a.Filename < b.Filename; });
        }
    }

    FileTreeNeedsRegen = false;
}

void FileExplorer::DrawFileNode(GuiState* state, FileExplorerNode& node)
{
    static u32 maxDoubleClickTime = 500; //Max ms between 2 clicks on one item to count as a double click
    static Timer clickTimer; //Measures times between consecutive clicks on the same node. Used to determine if a double click occurred

    //Make sure node fits search term
    if (!node.MatchesSearchTerm && !node.AnyChildNodeMatchesSearchTerm)
        return;

    //Draw node texxt
    f32 nodeXPos = ImGui::GetCursorPosX(); //Store position of the node for drawing the node icon later
    node.Open = ImGui::TreeNodeEx(node.Text.c_str(),
        //Make full node width clickable
        ImGuiTreeNodeFlags_SpanAvailWidth
        //If the node has no children make it a leaf node (a node which can't be expanded)
        | (node.Children.size() == 0 || !node.AnyChildNodeMatchesSearchTerm ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None)
        //Highlight the node if it's the currently selected node (the last node that was clicked)
        | (&node == state->FileExplorer_SelectedNode ? ImGuiTreeNodeFlags_Selected : 0));

    //Check if the node was clicked and detect double clicks
    if (ImGui::IsItemClicked())
    {
        //Behavior for double clicking a file
        if (state->FileExplorer_SelectedNode == &node)
        {
            //If time between clicks less than maxDoubleClickTime then a double click occurred
            if (clickTimer.ElapsedMilliseconds() < maxDoubleClickTime)
                DoubleClickedFile(state, node);

            clickTimer.Reset();
        }

        //Set FileExplorer_SelectedNode to most recently selected node
        state->FileExplorer_SelectedNode = &node;
        SingleClickedFile(state, node);
        clickTimer.Reset();
    }

    //Draw node icon
    ImGui::PushStyleColor(ImGuiCol_Text, GetNodeColor(node));
    ImGui::SameLine();
    ImGui::SetCursorPosX(nodeXPos + 22.0f);
    ImGui::Text(GetNodeIcon(node).c_str());
    ImGui::PopStyleColor();

    //If the node is open draw it's child nodes
    if (node.Open)
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
            DrawFileNode(state, childNode);
        }
        ImGui::TreePop();
    }
}

void FileExplorer::SingleClickedFile(GuiState* state, FileExplorerNode& node)
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

void FileExplorer::DoubleClickedFile(GuiState* state, FileExplorerNode& node)
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
        else if (extension == ".gvbm_pc")
            filename = Path::GetFileNameNoExtension(node.Filename) + ".cvbm_pc";

        state->CreateDocument(filename, CreateHandle<TextureDocument>(state, filename, node.ParentName, VppName, node.InContainer));
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

        state->CreateDocument(filename, CreateHandle<StaticMeshDocument>(state, filename, node.ParentName, VppName, node.InContainer));
    }
    else if (extension == ".xtbl")
    {
        state->CreateDocument(node.Filename, CreateHandle<XtblDocument>(state, node.Filename, node.ParentName, VppName, node.InContainer));
    }
    else if (extension == ".asm_pc")
    {
        state->CreateDocument(node.Filename, CreateHandle<AsmDocument>(state, node.Filename, node.ParentName, VppName, node.InContainer));
    }
    else if (extension == ".rfglocatext")
    {
        state->CreateDocument("Localization", CreateHandle<LocalizationDocument>(state));
    }
}

bool FileExplorer::FormatSupported(const string& ext)
{
    static const std::vector<string> supportedFormats = { /*".vpp_pc", ".str2_pc",*/ ".cpeg_pc", ".cvbm_pc", ".gpeg_pc", ".gvbm_pc", ".csmesh_pc", ".gsmesh_pc", ".ccmesh_pc", ".gcmesh_pc",
                                                            ".xtbl", ".asm_pc"};
    for (auto& format : supportedFormats)
    {
        if (ext == format)
            return true;
    }

    return false;
}

void FileExplorer::UpdateNodeSearchResultsRecursive(FileExplorerNode& node)
{
    node.MatchesSearchTerm = DoesNodeFitSearch(node);
    node.AnyChildNodeMatchesSearchTerm = AnyChildNodesFitSearch(node);
    for (auto& childNode : node.Children)
    {
        //Exit early if search thread signalled to stop
        if (searchTask_->Cancelled())
            return;

        UpdateNodeSearchResultsRecursive(childNode);
    }
}

//Get icon for node based on node type and extension
string FileExplorer::GetNodeIcon(FileExplorerNode& node)
{
    string ext = Path::GetExtension(node.Filename);
    if (ext == ".vpp_pc" || ext == ".str2_pc")
        return node.Open ? ICON_FA_FOLDER_OPEN " " : ICON_FA_FOLDER " ";
    else if (ext == ".asm_pc")
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

ImVec4 FileExplorer::GetNodeColor(FileExplorerNode& node)
{
    string ext = Path::GetExtension(node.Filename);
    ImVec4 defaultNodeColor = ImGui::GetStyle().Colors[ImGuiCol_Text];

    if (ext == ".vpp_pc" || ext == ".str2_pc")
        return { 235.0f / 255.0f, 199.0f / 255.0f, 96.0f / 255.0f, 1.0f };
    else if (ext == ".asm_pc")
        return { 240.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f };
    else if (ext == ".cpeg_pc" || ext == ".cvbm_pc" || ext == ".gpeg_pc" || ext == ".gvbm_pc")
        return { 100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f };
    else if (ext == ".xtbl")
        return { 186.0f / 255.0f, 117.0f / 255.0f, 182.0f / 255.0f, 1.0f };
    else if (ext == ".mtbl" || ext == ".dtodx" || ext == ".gtodx" || ext == ".scriptx" || ext == ".vint_proj" || ext == ".lua")
        return defaultNodeColor;
    else if (ext == ".rfgzone_pc" || ext == ".layer_pc")
        return defaultNodeColor;
    else if (ext == ".csmesh_pc" || ext == ".gsmesh_pc" || ext == ".ccmesh_pc" || ext == ".gcmesh_pc")
        return { 255.0f / 255.0f, 216.0f / 255.0f, 0.0f / 255.0f, 1.0f };
    else if (ext == ".ctmesh_pc" || ext == ".gtmesh_pc" || ext == ".cterrain_pc" || ext == ".gterrain_pc" || ext == ".cstch_pc" || ext == ".gstch_pc" || ext == ".cfmesh_pc")
        return defaultNodeColor;
    else if (ext == ".rig_pc")
        return defaultNodeColor;
    else if (ext == ".rfglocatext" || ext == ".le_strings")
        return defaultNodeColor;
    else if (ext == ".fxo_kg")
        return defaultNodeColor;
    else if (ext == ".ccar_pc" || ext == ".gcar_pc")
        return defaultNodeColor;
    else if (ext == ".vint_doc")
        return defaultNodeColor;
    else if (ext == ".anim_pc")
        return defaultNodeColor;
    else if (ext == ".sim_pc")
        return defaultNodeColor;
    else if (ext == ".cchk_pc" || ext == ".gchk_pc")
        return defaultNodeColor;
    else if (ext == ".fsmib")
        return defaultNodeColor;
    else if (ext == ".cefct_pc")
        return defaultNodeColor; // { 240.0f / 255.0f, 128.0f / 255.0f, 0.0f / 255.0f, 1.0f };
    else if (ext == ".mat_pc")
        return defaultNodeColor;
    else if (ext == ".morph_pc")
        return defaultNodeColor;
    else if (ext == ".xwb_pc" || ext == ".xsb_pc" || ext == ".xgs_pc" || ext == ".aud_pc" || ext == ".vfdvp_pc" || ext == ".rfgvp_pc")
        return defaultNodeColor;
    else if (ext == ".vf3_pc")
        return defaultNodeColor;
    else
        return defaultNodeColor;
}

//Returns true if the node text matches the current search term
bool FileExplorer::DoesNodeFitSearch(FileExplorerNode& node)
{
    //Hide unsupported formats
    if (HideUnsupportedFormats && !FormatSupported(Path::GetExtension(node.Filename)))
        return false;

    if (RegexSearch)
        return std::regex_search(node.Filename, SearchRegex);

    //Default search. Supports * wildcard prefix/postfix.
    if (CaseSensitive)
    {
        if (SearchType == Match && !String::Contains(node.Filename, SearchTermPatched))
            return false;
        else if (SearchType == MatchStart && !String::StartsWith(node.Filename, SearchTermPatched))
            return false;
        else if (SearchType == MatchEnd && !String::EndsWith(node.Filename, SearchTermPatched))
            return false;
        else
            return true;
    }
    else
    {
        if (SearchType == Match && !String::Contains(String::ToLower(node.Filename), String::ToLower(SearchTermPatched)))
            return false;
        else if (SearchType == MatchStart && !String::StartsWith(String::ToLower(node.Filename), String::ToLower(SearchTermPatched)))
            return false;
        else if (SearchType == MatchEnd && !String::EndsWith(String::ToLower(node.Filename), String::ToLower(SearchTermPatched)))
            return false;
        else
            return true;
    }
}

//Returns true if any of the child nodes of node match the current search term
bool FileExplorer::AnyChildNodesFitSearch(FileExplorerNode& node)
{
    for (auto& childNode : node.Children)
    {
        //Exit early if search thread signalled to stop
        if (searchTask_->Cancelled())
            return true;

        if (DoesNodeFitSearch(childNode) || AnyChildNodesFitSearch(childNode))
            return true;
    }
    return false;
}