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

    //Todo: Organize entries by category rather than showing them as one big list
    //Xtbl entry list
    if (ImGui::BeginChild("##EntryList"))
    {
        for (u32 i = 0; i < Xtbl->Entries.size(); i++)
        {
            Handle<XtblNode> entry = Xtbl->Entries[i];
            bool selected = SelectedIndex == i;

            //Try to get <Name></Name> value
            string name = entry->Name;
            auto nameValue = entry->GetSubnodeValueString("Name");
            if (nameValue)
                name = nameValue.value();

            string selectableName = fmt::format("{}##{}", name, i);

            if (ImGui::Selectable(selectableName.c_str(), selected))
            {
                SelectedIndex = i;
            }
        }
        ImGui::EndChild();
    }

    //Draw the selected entry values in column 1 (to the right of the entry list)
    ImGui::NextColumn();
    ImGui::SetCursorPosY(baseY);
    if (Xtbl->Entries.size() != 0 && ImGui::BeginChild("##EntryView", ImVec2(ImGui::GetColumnWidth(), ImGui::GetWindowHeight())))
    {
        if (SelectedIndex >= Xtbl->Entries.size())
            SelectedIndex = 0;

        Handle<XtblNode> entry = Xtbl->Entries[SelectedIndex];
        for (Handle<XtblNode> subnode : entry->Subnodes)
            DrawXtblNode(state, subnode, Xtbl, true);

        ImGui::EndChild();
    }

    ImGui::Columns(1);
    ImGui::End();
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
            ImGui::InputText(node->Name, node->Value.String);
            break;
        case XtblType::Int:
            ImGui::InputInt(node->Name.c_str(), &node->Value.Int);
            break;
        case XtblType::Float:
            ImGui::InputFloat(node->Name.c_str(), &node->Value.Float);
            break;
        case XtblType::Vector:
            ImGui::InputFloat3(node->Name.c_str(), (f32*)&node->Value.Vector);
            break;
        case XtblType::Color:
            ImGui::InputFloat3(node->Name.c_str(), (f32*)&node->Value.Vector);
            break;
        case XtblType::Selection:
            ImGui::InputText(node->Name, node->Value.String); //Todo: Replace with combo listing all values of Choice vector
            break;
        case XtblType::Filename:
            ImGui::InputText(node->Name, node->Value.String); //Todo: Should validate extension and try to find list of valid files
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
                for (auto& subnode : node->Subnodes)
                    DrawXtblNode(state, subnode, xtbl);

                ImGui::TreePop();
            }
            break;


        case XtblType::None:
        case XtblType::Reference:
            //Todo: Read values from other xtbl and list them 
            //break;
            //Default case is to display it as a constant string
        case XtblType::TableDescription:
        default:
            if (node->HasSubnodes())
            {
                if (ImGui::TreeNode(name.c_str()))
                {
                    for (auto& subnode : node->Subnodes)
                        DrawXtblNode(state, subnode, xtbl);

                    ImGui::TreePop();
                }
            }
            else
            {
                gui::LabelAndValue(name + ":", node->Value.String);
            }
            break;
        }
        ImGui::PopItemWidth();
    }
    else
    {
        gui::LabelAndValue(name + ":", node->Value.String);
    }
}
