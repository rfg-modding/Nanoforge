#include "XtblDocument.h"
#include "Log.h"
#include "render/imgui/imgui_ext.h"
#include "application/project/Project.h"
#include "render/imgui/imgui_ext.h"
#include "gui/GuiState.h"
#include "rfg/xtbl/XtblNodes.h"
#include "gui/util/WinUtil.h"
#include "rfg/xtbl/XtblManager.h"
#include "rfg/xtbl/Xtbl.h"
#include "util/Profiler.h"
#include "render/imgui/ImGuiFontManager.h"
#include <filesystem>

XtblDocument::XtblDocument(GuiState* state, std::string_view filename, std::string_view parentName, std::string_view vppName, bool inContainer, IXtblNode* startingNode)
    : filename_(filename), parentName_(parentName), vppName_(vppName), inContainer_(inContainer), state_(state)
{
    xtblManager_ = state->Xtbls;
    bool result = state->Xtbls->ParseXtbl(vppName_, filename_);
    if (!result)
    {
        LOG_ERROR("Failed to parse {}. Closing xtbl document.", filename_);
        Open = false;
        return;
    }

    xtbl_ = state->Xtbls->GetXtbl(vppName_, filename_);
    if (!xtbl_)
    {
        LOG_ERROR("Failed to get {} from XtblManager. Closing xtbl document.", filename_);
        Open = false;
        return;
    }

    auto firstEntry = xtbl_->Entries.size() > 0 ? xtbl_->Entries[0] : nullptr;
    SelectedNode = startingNode ? startingNode : firstEntry;
}

XtblDocument::~XtblDocument()
{
    //If document is closed and changes are discarded reload the xtbl to reset it's changes
    //Can't just delete the xtbl since other xtbls could reference this one
    if (ResetOnClose)
    {
        xtbl_->Reload(state_->PackfileVFS);
    }
}

void XtblDocument::Update(GuiState* state)
{
    PROFILER_FUNCTION();

    static bool showedNoProjectWarning = false;
    if (!state->CurrentProject->Loaded() && !showedNoProjectWarning)
    {
        ShowMessageBox("You need to have a project open to edit xtbls. Please open a project or create a new one via `File > New project` in the main menu bar.", "No project open", MB_OK);
        showedNoProjectWarning = true;
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

    //Draw search bar and buttons on sidebar
    ImGui::InputText("Search", searchTerm_);
    if (ImGui::Button(ICON_FA_PLUS)) //Add entry
        AddEntry();

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FOLDER_PLUS)) //Add category
        AddCategory();

    ImGui::Separator();

    //Draw list of entries
    if (ImGui::BeginChild("##EntryList"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 1.40f); //Increase spacing to differentiate leaves from expanded contents.

        //Note: Iterated by index so items can be added while iterating
        //Draw categorized nodes
        for (u32 i = 0; i < xtbl_->RootCategory->SubCategories.size(); i++)
            DrawXtblCategory(xtbl_->RootCategory->SubCategories[i]);

        //Draw uncategorized nodes
        for (u32 i = 0; i < xtbl_->RootCategory->Nodes.size(); i++)
            DrawXtblNodeEntry(xtbl_->RootCategory->Nodes[i]);

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
        }

        //Draw editors for subnodes
        ImGui::SetCursorPosY(dataY);
        if (ImGui::BeginChild("##EntryView", { 0, 0 }, false, ImGuiWindowFlags_HorizontalScrollbar))
        {
            //Subnodes are drawn by description so empty optional elements are visible
            ImGui::PushItemWidth(NodeGuiWidth);
            for (auto& desc : xtbl_->TableDescription->Subnodes)
                if (DrawNodeByDescription(state, xtbl_, desc, SelectedNode))
                    UnsavedChanges = true;

            ImGui::PopItemWidth();
            ImGui::EndChild();
        }
    }

    ImGui::Columns(1);

    //Draw popup windows if they're open
    DrawRenameCategoryWindow();
    DrawRenameEntryWindow();
}

#pragma warning(disable:4100)
void XtblDocument::Save(GuiState* state)
{
    //Don't save if xtbl_ is null or no subnodes have been edited or no project is open
    if (!state_->CurrentProject->Loaded())
    {
        LOG_ERROR("Failed to save {}. No project is open. Open a project or create a new one to be able to edit xtbls.", filename_);
        return;
    }
    if (!xtbl_ || !xtbl_->PropagateEdits())
        return;

    //Path to output folder in the project cache
    string outputFolderPath = state_->CurrentProject->GetCachePath() + xtbl_->VppName + "\\";
    string outputFilePath = outputFolderPath + xtbl_->Name;

    //Ensure output folder exists
    std::filesystem::create_directories(outputFolderPath);

    //Save xtbl project cache and rescan cache to see edited files in it
    xtbl_->WriteXtbl(outputFilePath);
    state_->CurrentProject->RescanCache();
}
#pragma warning(default:4100)

bool XtblDocument::AnyChildMatchesSearchTerm(Handle<XtblCategory> category)
{
    for (auto& subcategory : category->SubCategories)
        if (String::Contains(subcategory->Name, searchTerm_) || AnyChildMatchesSearchTerm(subcategory))
            return true;

    for (auto& subnode : category->Nodes)
    {
        auto name = subnode->GetSubnodeValueString("Name");
        if (!name.has_value())
            continue;
        if (String::Contains(String::ToLower(name.value()), String::ToLower(searchTerm_)))
            return true;
    }

    return false;
}

void XtblDocument::DrawXtblCategory(Handle<XtblCategory> category, bool openByDefault)
{
    //Don't draw node if neither it nor it's children match the search
    if (searchTerm_ != "" && !String::Contains(category->Name, searchTerm_) && !AnyChildMatchesSearchTerm(category))
        return;

    //Draw category node
    f32 nodeXPos = ImGui::GetCursorPosX(); //Store position of the node for drawing the node icon later
    bool nodeOpen = ImGui::TreeNodeEx(fmt::format("      {}##{}", category->Name, (u64)category.get()).c_str(),
        //Make full node width clickable
        ImGuiTreeNodeFlags_SpanAvailWidth
        //If the node has no children make it a leaf node (a node which can't be expanded)
        | (category->SubCategories.size() + category->Nodes.size() == 0 ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None
        | (openByDefault ? ImGuiTreeNodeFlags_DefaultOpen : 0)));

    //Draw right click context menu
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Rename"))
        {
            renameCategory_ = category;
            renameCategoryName_ = category->Name;
            renameCategoryWindowOpen_ = true;
        }

        ImGui::EndPopup();
    }

    //Draw node icon
    const ImVec4 folderIconColor = { 235.0f / 255.0f, 199.0f / 255.0f, 96.0f / 255.0f, 1.0f };;
    ImGui::PushStyleColor(ImGuiCol_Text, folderIconColor);
    ImGui::SameLine();
    ImGui::SetCursorPosX(nodeXPos + 22.0f);
    ImGui::Text(nodeOpen ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER);
    ImGui::PopStyleColor();

    //Draw subcategories and subnodes
    if (nodeOpen)
    {
        //Note: Iterated by index so items can be added while iterating
        //Draw subcategories and their nodes recursively
        for (u32 i = 0; i < category->SubCategories.size(); i++)
            DrawXtblCategory(category->SubCategories[i]);

        //Draw subnodes
        for (u32 i = 0; i < category->Nodes.size(); i++)
            DrawXtblNodeEntry(category->Nodes[i]);

        ImGui::TreePop();
    }
}

void XtblDocument::DrawXtblNodeEntry(IXtblNode* node)
{
    //Don't draw node if it doesn't match the search
    auto nameNodeValue = node->GetSubnodeValueString("Name");
    if (searchTerm_ != "" && (nameNodeValue.has_value() && !String::Contains(String::ToLower(nameNodeValue.value()), String::ToLower(searchTerm_))))
        return;

    //Try to get <Name></Name> value
    string name = node->Name;
    auto nameValue = node->GetSubnodeValueString("Name");
    if (nameValue)
        name = nameValue.value();

    //Draw node
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf
                               | (SelectedNode == node ? ImGuiTreeNodeFlags_Selected : 0);
    bool nodeDrawn = ImGui::TreeNodeEx(fmt::format("{} {}##{}", ICON_FA_CODE, name, (u64)node).c_str(), flags);

    //Draw right click context menu
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Rename"))
        {
            auto nameNode = node->GetSubnode("Name");
            if (nameNode)
            {
                renameEntry_ = node;
                renameEntryName_ = std::get<string>(nameNode->Value);
                renameEntryWindowOpen_ = true;
            }
        }
        if (ImGui::MenuItem("Duplicate"))
        {
            DuplicateEntry(node);
        }

        ImGui::EndPopup();
    }

    //Handle left click behavior
    if (nodeDrawn)
    {
        if (ImGui::IsItemClicked())
            SelectedNode = node;

        ImGui::TreePop();
    }
}

void XtblDocument::AddEntry()
{
    //If a node is already selected put the new node in the selected nodes category
    string category = SelectedNode ? xtbl_->GetNodeCategoryPath(SelectedNode) : "";
    Handle<XtblDescription> desc = xtbl_->TableDescription;

    //Create the new node and set it's name
    IXtblNode* newEntry = CreateDefaultNode(desc, true);
    auto nameNode = newEntry->GetSubnode("Name");
    if (nameNode)
    {
        nameNode->Enabled = true;
        nameNode->Value = xtbl_->TableDescription->Name;
    }
    newEntry->Edited = true;
    SelectedNode = newEntry;

    //Set the nodes category and add it to the xtbl
    xtbl_->Entries.push_back(newEntry);
    xtbl_->SetNodeCategory(newEntry, category);
}

void XtblDocument::AddCategory()
{
    //If node is selected make the category a subcategory of the nodes category
    if (SelectedNode && xtbl_->GetNodeCategory(SelectedNode))
    {
        Handle<XtblCategory> parent = xtbl_->GetNodeCategory(SelectedNode);
        auto newCategory = CreateHandle<XtblCategory>("New category");
        parent->SubCategories.push_back(newCategory);
    }
    else //If no node is selected add a category to the root category
    {
        Handle<XtblCategory> parent = xtbl_->RootCategory;
        auto newCategory = CreateHandle<XtblCategory>("New category");
        parent->SubCategories.push_back(newCategory);
    }
}

void XtblDocument::DuplicateEntry(IXtblNode* entry)
{
    auto newEntry = entry->DeepCopy();
    newEntry->Edited = true;
    newEntry->NewEntry = true;
    xtbl_->Entries.push_back(newEntry);
    xtbl_->SetNodeCategory(newEntry, xtbl_->GetNodeCategoryPath(entry));
    SelectedNode = newEntry;

    //Append " (copy)" to new entry name
    auto nameNode = newEntry->GetSubnode("Name");
    if (nameNode)
        nameNode->Value = std::get<string>(nameNode->Value) + " (copy)";
}

void XtblDocument::DrawRenameCategoryWindow()
{
    if (renameCategoryWindowOpen_)
        ImGui::OpenPopup(renameCategoryPopupId_);

    ImGui::SetNextWindowSize({ 600.0f, 200.0f }, ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(renameCategoryPopupId_))
    {
        //Close window if valid node isn't selected
        if (!renameCategory_)
        {
            renameCategoryWindowOpen_ = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        //Update name if edited
        ImGui::InputText("Name", renameCategoryName_);

        //Draw save and cancel buttons
        if (ImGui::Button("Save"))
        {
            //Rename category
            xtbl_->RenameCategory(renameCategory_, renameCategoryName_);

            //Reset popup state
            renameCategoryWindowOpen_ = false;
            renameCategory_ = nullptr;
            renameCategoryName_ = "";
            ImGui::CloseCurrentPopup();

            //Todo: Add support for preserving categories with no subnodes by reading categories from <EntryCategories> in Xtbl.cpp
            //Mark xtbl as edited and save the xtbl to preserve the rename
            if (renameCategory_->Nodes.size() > 0)
            {
                renameCategory_->Nodes[0]->Edited = true;
                UnsavedChanges = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            renameCategoryWindowOpen_ = false;
            renameCategory_ = nullptr;
            renameCategoryName_ = "";
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    else
    {
        renameCategory_ = nullptr;
        renameCategoryName_ = "";
    }
}

void XtblDocument::DrawRenameEntryWindow()
{
    if (renameEntryWindowOpen_)
        ImGui::OpenPopup(renameEntryPopupId_);

    ImGui::SetNextWindowSize({ 600.0f, 200.0f }, ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(renameEntryPopupId_))
    {
        //Close window if no node is selected
        if (!renameEntry_ || !renameEntry_->GetSubnode("Name"))
        {
            renameEntryWindowOpen_ = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        //Edit name
        bool enterPressed = ImGui::InputText("Name", renameEntryName_, ImGuiInputTextFlags_EnterReturnsTrue);

        //Draw save and cancel buttons
        if (ImGui::Button("Save") || enterPressed)
        {
            //Set node name
            IXtblNode* nameNode = renameEntry_->GetSubnode("Name");
            nameNode->Value = renameEntryName_;
            nameNode->Edited = true;

            //Reset popup state
            renameEntry_ = nullptr;
            renameEntryName_ = "";
            renameEntryWindowOpen_ = false;
            ImGui::CloseCurrentPopup();

            UnsavedChanges = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            renameEntry_ = nullptr;
            renameEntryName_ = "";
            renameEntryWindowOpen_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    else
    {
        renameEntry_ = nullptr;
        renameEntryName_ = "";
    }
}