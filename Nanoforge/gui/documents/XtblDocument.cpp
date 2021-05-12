#include "XtblDocument.h"
#include "Log.h"
#include "render/imgui/imgui_ext.h"
#include "gui/GuiState.h"
#include "rfg/xtbl/XtblNodes.h"
#include "rfg/xtbl/XtblManager.h"
#include "rfg/xtbl/Xtbl.h"

XtblDocument::XtblDocument(GuiState* state, string filename, string parentName, string vppName, bool inContainer)
    : filename_(filename), parentName_(parentName), vppName_(vppName), inContainer_(inContainer)
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
}

XtblDocument::~XtblDocument()
{

}

void XtblDocument::Update(GuiState* state)
{
    if (!ImGui::Begin(Title.c_str(), &open_, ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::End();
        return;
    }

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, 300.0f);

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_CODE " Entries");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    //Save cursor y at start of list to align columns
    f32 baseY = ImGui::GetCursorPosY();

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

    //Draw values of the selected entry
    ImGui::SetCursorPosY(baseY);
    ImGui::NextColumn();
    if (xtbl_->Entries.size() != 0 && selectedNode_ && ImGui::BeginChild("##EntryView"))
    {
        //Draw subnodes with descriptions so empty optional elements are visible
        ImGui::PushItemWidth(NodeGuiWidth);
        for (auto& node : selectedNode_->Subnodes)
            node->DrawEditor(state->Xtbls, xtbl_);
    
        ImGui::PopItemWidth();
        ImGui::EndChild();
    }

    ImGui::Columns(1);
    ImGui::End();
}

void XtblDocument::DrawXtblCategory(Handle<XtblCategory> category, bool openByDefault)
{
    //Draw category node
    bool nodeOpen = ImGui::TreeNodeEx(fmt::format("{} {}",ICON_FA_FOLDER, category->Name).c_str(),
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
                               | (selectedNode_ == node ? ImGuiTreeNodeFlags_Selected : 0);
    if (ImGui::TreeNodeEx(fmt::format("{} {}", ICON_FA_CODE, name).c_str(), flags))
    {
        if (ImGui::IsItemClicked())
            selectedNode_ = node;

        ImGui::TreePop();
    }
}