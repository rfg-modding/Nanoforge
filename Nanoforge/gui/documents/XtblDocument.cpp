#include "XtblDocument.h"
#include "Log.h"
#include "render/imgui/imgui_ext.h"
#include "application/project/Project.h"
#include "gui/GuiState.h"
#include "rfg/xtbl/XtblNodes.h"
#include "rfg/xtbl/XtblManager.h"
#include "rfg/xtbl/Xtbl.h"

XtblDocument::XtblDocument(GuiState* state, string filename, string parentName, string vppName, bool inContainer, Handle<IXtblNode> startingNode)
    : filename_(filename), parentName_(parentName), vppName_(vppName), inContainer_(inContainer), state_(state)
{
    xtblManager_ = state->Xtbls;
    bool result = state->Xtbls->ParseXtbl(vppName_, filename_);
    if (!result)
    {
        Log->error("Failed to parse {}. Closing xtbl document.", filename_);
        open_ = false;
        return;
    }

    xtbl_ = state->Xtbls->GetXtbl(vppName_, filename_);
    if (!xtbl_)
    {
        Log->error("Failed to get {} from XtblManager. Closing xtbl document.", filename_);
        open_ = false;
        return;
    }

    auto firstEntry = xtbl_->Entries.size() > 0 ? xtbl_->Entries[0] : nullptr;
    SelectedNode = startingNode ? startingNode : firstEntry;
}

XtblDocument::~XtblDocument()
{
    //Todo: Replace this with a more robust saving system where the user is warned when they have unsaved data and each document can be saved separately
    //Auto save xtbl on close to the project cache
    Save();
}

void XtblDocument::Update(GuiState* state)
{
    if (!ImGui::Begin(Title.c_str(), &open_, ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::End();
        return;
    }

    ImGui::Columns(2);
    if (FirstDraw)
        ImGui::SetColumnWidth(0, 300.0f);

    f32 headerY = ImGui::GetCursorPosY();
    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_FILE_ALT " Entries");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    //Save cursor y at start of list to align columns
    f32 dataY = ImGui::GetCursorPosY();

    //Draw sidebar with list of entries
    if (ImGui::BeginChild("##EntryList"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 0.75f); //Increase spacing to differentiate leaves from expanded contents.
        for (auto& subcategory : xtbl_->RootCategory->SubCategories) //Draw categorized nodes
            DrawXtblCategory(subcategory);
        for (auto& subnode : xtbl_->RootCategory->Nodes) //Draw uncategorized nodes
            DrawXtblNodeEntry(subnode);

        ImGui::PopStyleVar();
        ImGui::EndChild();
    }

    //Draw editor for selected node
    ImGui::NextColumn();
    if (xtbl_->Entries.size() != 0 && SelectedNode)
    {
        //Draw the name of the node if it has one
        ImGui::SetCursorPosY(headerY);
        auto selectedNodeName = SelectedNode->GetSubnodeValueString("Name");
        if (selectedNodeName)
        {
            state->FontManager->FontL.Push();
            ImGui::Text(fmt::format("{} {}", ICON_FA_CODE, selectedNodeName.value()).c_str());
            state->FontManager->FontL.Pop();

            //Save button + tooltip describing how xtbl saving works at the moment
            ImGui::SameLine();
            if (ImGui::Button("Save"))
            {
                Save();
            }
            gui::TooltipOnPrevious("Xtbl documents are automatically saved when you close them or package your mod. Press this to manually save them. An upcoming release will have a better save system along with warnings when you have unsaved data.", state->FontManager->FontDefault.GetFont());
        }

        //Draw editors for subnodes
        ImGui::SetCursorPosY(dataY);
        if (ImGui::BeginChild("##EntryView"))
        {
            //Subnodes are drawn by description so empty optional elements are visible
            ImGui::PushItemWidth(NodeGuiWidth);
            for (auto& desc : xtbl_->TableDescription->Subnodes)
                DrawNodeByDescription(state, xtbl_, desc, SelectedNode);

            ImGui::PopItemWidth();
            ImGui::EndChild();
        }
    }

    ImGui::Columns(1);
    ImGui::End();
}

void XtblDocument::DrawXtblCategory(Handle<XtblCategory> category, bool openByDefault)
{
    //Draw category node
    bool nodeOpen = ImGui::TreeNodeEx(fmt::format("{} {}", ICON_FA_FOLDER, category->Name).c_str(),
        //Make full node width clickable
        ImGuiTreeNodeFlags_SpanAvailWidth
        //If the node has no children make it a leaf node (a node which can't be expanded)
        | (category->SubCategories.size() + category->Nodes.size() == 0 ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None 
        | (openByDefault ? ImGuiTreeNodeFlags_DefaultOpen : 0)));

    //Draw subcategories and subnodes
    if (nodeOpen)
    {
        //Draw subcategories and their nodes recursively
        for (auto& subcategory : category->SubCategories)
            DrawXtblCategory(subcategory);

        //Draw subnodes
        for (auto& subnode : category->Nodes)
            DrawXtblNodeEntry(subnode);

        ImGui::TreePop();
    }
}

void XtblDocument::DrawXtblNodeEntry(Handle<IXtblNode> node)
{
    //Try to get <Name></Name> value
    string name = node->Name;
    auto nameValue = node->GetSubnodeValueString("Name");
    if (nameValue)
        name = nameValue.value();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf 
                               | (SelectedNode == node ? ImGuiTreeNodeFlags_Selected : 0);
    if (ImGui::TreeNodeEx(fmt::format("{} {}", ICON_FA_CODE, name).c_str(), flags))
    {
        if (ImGui::IsItemClicked())
            SelectedNode = node;

        ImGui::TreePop();
    }
}

void XtblDocument::Save()
{
    if (xtbl_)
    {
        //Path to output folder in the project cache
        string outputFolderPath = state_->CurrentProject->GetCachePath() + xtbl_->VppName + "\\";
        string outputFilePath = outputFolderPath + xtbl_->Name;

        //Ensure output folder exists
        std::filesystem::create_directories(outputFolderPath);

        //Save xtbl project cache and rescan cache to see edited files in it
        xtbl_->WriteXtbl(outputFilePath);
        state_->CurrentProject->RescanCache();
    }
}
