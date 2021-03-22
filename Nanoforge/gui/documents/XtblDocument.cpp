#include "XtblDocument.h"
#include "Log.h"
#include "render/imgui/imgui_ext.h"
#include "gui/GuiState.h"

XtblDocument::XtblDocument(GuiState* state, string filename, string parentName, string vppName, bool inContainer)
    : filename_(filename), parentName_(parentName), vppName_(vppName), inContainer_(inContainer)
{
    bool result = state->Xtbls->ParseXtbl(vppName_, filename_);
    if (!result)
    {
        Log->error("Failed to parse {}. Closing xtbl document.", filename_);
        open_ = false;
        return;
    }

    auto optional = state->Xtbls->GetXtbl(vppName_, filename_);
    if (!optional)
    {
        Log->error("Failed to get {} from XtblManager. Closing xtbl document.", filename_);
        open_ = false;
        return;
    }

    xtbl_ = optional.value();
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

    //Save cursor y at start of list so we can align the entry
    f32 baseY = ImGui::GetCursorPosY();

    //Xtbl entry list
    if (ImGui::BeginChild("##EntryList"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 0.75f); //Increase spacing to differentiate leaves from expanded contents.

        //Draw subcategories
        for (auto& subcategory : xtbl_->RootCategory->SubCategories)
            DrawXtblCategory(subcategory);

        //Draw subnodes
        for (auto& subnode : xtbl_->RootCategory->Nodes)
            DrawXtblNodeEntry(subnode);

        ImGui::PopStyleVar();
        ImGui::EndChild();
    }

    //Draw the selected entry values in column 1 (to the right of the entry list)
    ImGui::NextColumn();
    ImGui::SetCursorPosY(baseY);
    if (xtbl_->Entries.size() != 0 && selectedNode_ && ImGui::BeginChild("##EntryView"))
    {
        for (auto& subnode : selectedNode_->Subnodes)
            DrawXtblNode(subnode);

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

void XtblDocument::DrawXtblNodeEntry(Handle<XtblNode> node)
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

void XtblDocument::DrawXtblNode(Handle<XtblNode> node, const char* nameOverride)
{
    //Get name
    string nameNoId = nameOverride ? nameOverride : node->Name; //Store copy of name without unique ID appended. A few types use ImGui elements that don't support ##IDs
    string name = nameNoId + fmt::format("##{}", (u64)node.get()); //Append unique ID. Uses address of node since that value is unique and unchanging.
    if (nameNoId == "_Editor") //Don't draw editor settings block. Usually just has category used by entry list on the sidebar
        return;

    //Get description. Use default draw method if no description is present
    auto maybeDesc = xtbl_->GetValueDescription(node->GetPath());
    if (!maybeDesc)
    {
        gui::LabelAndValue(nameNoId + ":", std::get<string>(node->Value));
        return;
    }

    //Draw node
    ImGui::PushItemWidth(240.0f);
    XtblDescription desc = maybeDesc.value();
    switch (desc.Type)
    {
    case XtblType::String:
        ImGui::InputText(name, std::get<string>(node->Value));
        break;
    case XtblType::Int:
        ImGui::InputInt(name.c_str(), &std::get<i32>(node->Value));
        break;
    case XtblType::Float:
        ImGui::InputFloat(name.c_str(), &std::get<f32>(node->Value));
        break;
    case XtblType::Vector:
        ImGui::InputFloat3(name.c_str(), (f32*)&std::get<Vec3>(node->Value));
        break;
    case XtblType::Color:
        ImGui::ColorPicker3(name.c_str(), (f32*)&std::get<Vec3>(node->Value));
        break;
    case XtblType::Selection:
        ImGui::InputText(name, std::get<string>(node->Value)); //Todo: Replace with combo listing all values of Choice vector
        break;
    case XtblType::Filename:
        ImGui::InputText(name, std::get<string>(node->Value)); //Todo: Should validate extension and try to find list of valid files
        break;
    case XtblType::ComboElement:
        //Todo: Determine which type is being used and list proper ui elements. This type is like a union in c/c++
        //break;
    case XtblType::Flag: //Todo: Use checkbox instead of just listing the string value
        gui::LabelAndValue(nameNoId + ":", std::get<string>(node->Value));
        break;
    case XtblType::Flags:
        //Todo: Use list of checkboxes for each flag
        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc.Description != "")
                gui::TooltipOnPrevious(desc.Description, nullptr);
            for (auto& subnode : node->Subnodes)
            {
                //Try to get <Name></Name> value
                string subnodeName = subnode->Name;
                auto nameValue = subnode->GetSubnodeValueString("Name");
                if (nameValue)
                    subnodeName = nameValue.value();

                DrawXtblNode(subnode, subnodeName.c_str());
            }

            ImGui::TreePop();
        }
        break;
    case XtblType::List:
        //Draw subnodes
        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc.Description != "")
                gui::TooltipOnPrevious(desc.Description, nullptr);
            for (auto& subnode : node->Subnodes)
            {
                //Try to get <Name></Name> value
                string subnodeName = subnode->Name;
                auto nameValue = subnode->GetSubnodeValueString("Name");
                if (nameValue)
                    subnodeName = nameValue.value();

                DrawXtblNode(subnode, subnodeName.c_str());
            }

            ImGui::TreePop();
        }
        break;
    case XtblType::Grid:
        //List of elements
        //break;
    case XtblType::Element:
        //Draw subnodes
        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc.Description != "")
                gui::TooltipOnPrevious(desc.Description, nullptr);
            for (auto& subnode : node->Subnodes)
                DrawXtblNode(subnode);

            ImGui::TreePop();
        }
        break;


    case XtblType::None:
    case XtblType::Reference:
        //Todo: Read values from other xtbl and list them 
        gui::LabelAndValue(nameNoId + ":", std::get<string>(node->Value) + " (reference)");
        break;
    case XtblType::TableDescription:
    default:
        if (node->HasSubnodes())
        {
            if (ImGui::TreeNode(name.c_str()))
            {
                if (desc.Description != "")
                    gui::TooltipOnPrevious(desc.Description, nullptr);
                for (auto& subnode : node->Subnodes)
                    DrawXtblNode(subnode);

                ImGui::TreePop();
            }
        }
        else
        {
            gui::LabelAndValue(nameNoId + ":", std::get<string>(node->Value));
        }
        break;
    }

    //Draw tooltip if not empty
    if (desc.Description != "")
        gui::TooltipOnPrevious(desc.Description, nullptr);

    ImGui::PopItemWidth();
}