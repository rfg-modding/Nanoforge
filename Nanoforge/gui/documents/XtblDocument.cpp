#include "XtblDocument.h"
#include "Log.h"
#include "render/imgui/imgui_ext.h"
#include "gui/GuiState.h"

XtblDocument::XtblDocument(GuiState* state, string filename, string parentName, string vppName, bool inContainer)
    : Filename(filename), ParentName(parentName), VppName(vppName), InContainer(inContainer)
{
    bool result = state->Xtbls->ParseXtbl(VppName, Filename);
    if (!result)
    {
        Log->error("Failed to parse {}. Closing xtbl document.", Filename);
        open_ = false;
        return;
    }

    auto optional = state->Xtbls->GetXtbl(VppName, Filename);
    if (!optional)
    {
        Log->error("Failed to get {} from XtblManager. Closing xtbl document.", Filename);
        open_ = false;
        return;
    }

    Xtbl = optional.value();
}

XtblDocument::~XtblDocument()
{
}

void XtblDocument::Update(GuiState* state)
{
    if (!ImGui::Begin(Title.c_str(), &open_))
    {
        ImGui::End();
        return;
    }

    const f32 columnZeroWidth = 300.0f;
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, columnZeroWidth);

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_CODE " Entries");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    //Save cursor y at start of list so we can align the entry
    f32 baseY = ImGui::GetCursorPosY();
    //Calculate total height of entry list. Used if less than the limit to avoid having a bunch of empty space
    f32 entryHeight = ImGui::GetFontSize() + (ImGui::GetStyle().FramePadding.y * 2.0f);
    f32 listHeightTotal = Xtbl->Entries.size() * entryHeight;

    //Xtbl entry list
    if (ImGui::BeginChild("##EntryList"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 0.75f); //Increase spacing to differentiate leaves from expanded contents.

        //Draw subcategories
        for (auto& subcategory : Xtbl->RootCategory->SubCategories)
            DrawXtblCategory(state, subcategory);

        //Draw subnodes
        for (auto& subnode : Xtbl->RootCategory->Nodes)
            DrawXtblNodeEntry(state, subnode);

        ImGui::PopStyleVar();
        ImGui::EndChild();
    }

    //Draw the selected entry values in column 1 (to the right of the entry list)
    ImGui::NextColumn();
    ImGui::SetCursorPosY(baseY);
    if (Xtbl->Entries.size() != 0 && selectedNode_ && ImGui::BeginChild("##EntryView", ImVec2(ImGui::GetColumnWidth(), ImGui::GetWindowHeight())))
    {
        for (Handle<XtblNode> subnode : selectedNode_->Subnodes)
            DrawXtblNode(state, subnode, Xtbl, true);

        ImGui::EndChild();
    }

    ImGui::Columns(1);
    ImGui::End();
}

void XtblDocument::DrawXtblCategory(GuiState* state, Handle<XtblCategory> category, bool openByDefault)
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
            DrawXtblCategory(state, subcategory);

        //Draw subnodes
        for (auto& subnode : category->Nodes)
            DrawXtblNodeEntry(state, subnode);

        ImGui::TreePop();
    }
}

void XtblDocument::DrawXtblNodeEntry(GuiState* state, Handle<XtblNode> node)
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

void XtblDocument::DrawXtblNode(GuiState* state, Handle<XtblNode> node, Handle<XtblFile> xtbl, bool rootNode)
{
    string name = node->Name;
    if (rootNode) //Try to get <Name></Name> value if this is a root node and use that as the node name
    {
        auto nameValue = node->GetSubnodeValueString("Name");
        if (nameValue)
            name = nameValue.value();
    }

    //Todo: Add support for description addons to fill in for missing descriptions. These would be separate files shipped with Nanoforge that supplement and/or replace description included with the game
    auto maybeDesc = xtbl->GetValueDescription(node->GetPath());
    if (maybeDesc)
    {
        ImGui::PushItemWidth(240.0f);
        XtblDescription desc = maybeDesc.value();
        switch (desc.Type)
        {
        case XtblType::String:
            ImGui::InputText(node->Name, std::get<string>(node->Value));
            break;
        case XtblType::Int:
            ImGui::InputInt(node->Name.c_str(), &std::get<i32>(node->Value));
            break;
        case XtblType::Float:
            ImGui::InputFloat(node->Name.c_str(), &std::get<f32>(node->Value));
            break;
        case XtblType::Vector:
            ImGui::InputFloat3(node->Name.c_str(), (f32*)&std::get<Vec3>(node->Value));
            break;
        case XtblType::Color:
            ImGui::ColorPicker3(node->Name.c_str(), (f32*)&std::get<Vec3>(node->Value));
            break;
        case XtblType::Selection:
            ImGui::InputText(node->Name, std::get<string>(node->Value)); //Todo: Replace with combo listing all values of Choice vector
            break;
        case XtblType::Filename:
            ImGui::InputText(node->Name, std::get<string>(node->Value)); //Todo: Should validate extension and try to find list of valid files
            break;

        case XtblType::ComboElement:
            //Todo: Determine which type is being used and list proper ui elements. This type is like a union in c/c++
            //break;
        case XtblType::Flags:
            //Todo: Use list of checkboxes for each flag
            //break;
        case XtblType::List:
            //Todo: List of values
            //break;
        case XtblType::Grid:
            //List of elements
            //break;
        case XtblType::Element:
            //Draw subnodes
            if (ImGui::TreeNode(name.c_str()))
            {
                gui::TooltipOnPrevious(desc.Description, nullptr);
                for (auto& subnode : node->Subnodes)
                    DrawXtblNode(state, subnode, xtbl);

                ImGui::TreePop();
            }
            break;


        case XtblType::None:
        case XtblType::Reference:
            //Todo: Read values from other xtbl and list them 
            gui::LabelAndValue(name + ":", std::get<string>(node->Value) + " (reference)");
            break;
        case XtblType::TableDescription:
        default:
            if (node->HasSubnodes())
            {
                if (ImGui::TreeNode(name.c_str()))
                {
                    gui::TooltipOnPrevious(desc.Description, nullptr);
                    for (auto& subnode : node->Subnodes)
                        DrawXtblNode(state, subnode, xtbl);

                    ImGui::TreePop();
                }
            }
            else
            {
                gui::LabelAndValue(name + ":", std::get<string>(node->Value));
            }
            break;
        }
        gui::TooltipOnPrevious(desc.Description, nullptr);
        ImGui::PopItemWidth();
    }
    else if(name != "_Editor")
    {
        gui::LabelAndValue(name + ":", std::get<string>(node->Value));
    }
}
