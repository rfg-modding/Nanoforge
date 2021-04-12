#include "XtblDocument.h"
#include "Log.h"
#include "render/imgui/imgui_ext.h"
#include "gui/GuiState.h"
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

    //Todo: Things to do:
    //Todo: Add button to jump to xtbl references
    //Todo: Add support for remaining xtbl types
    //Todo: Track xtbl changes
    //Todo: Add xtbl packaging so you can edit xtbls in Nanoforge and generate a MM mod
    //Todo: Add support for description addons to fill in for missing descriptions
    //Todo: Add difference viewer for xtbl files

    //Draw the selected entry values in column 1 (to the right of the entry list)
    ImGui::SetCursorPosY(baseY);
    ImGui::NextColumn();
    if (xtbl_->Entries.size() != 0 && selectedNode_ && ImGui::BeginChild("##EntryView"))
    {
        //Todo: Report/log elements without documentation so we know to supplement it
        //Draw subnodes with descriptions so empty optional elements are visible
        for (auto& subnode : xtbl_->TableDescription->Subnodes)
            DrawXtblEntry(subnode);
    
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

void XtblDocument::DrawXtblEntry(Handle<XtblDescription> desc, const char* nameOverride, Handle<XtblNode> nodeOverride)
{
    string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
    string path(desc->GetPath());
    string checkboxId = fmt::format("##{}", (u64)desc.get());
    string descPath = desc->GetPath();

    Handle<XtblNode> node = nullptr;
    if (nodeOverride)
        node = nodeOverride;
    else
        node = xtbl_->GetSubnode(descPath.substr(descPath.find_first_of('/') + 1), selectedNode_);

    bool nodePresent = true;
    if (node == nullptr)
        nodePresent = false;
    else if (!node->Enabled)
        nodePresent = false;

    if (!desc->Required)
    {
        if (ImGui::Checkbox(checkboxId.c_str(), &nodePresent))
        {
            //When checkbox is set to true ensure elements exist in node
            if (nodePresent)
            {
                xtbl_->EnsureEntryExists(desc, selectedNode_);
                return;
            }
            else
            {
                node->Enabled = false;
            }
        }
        ImGui::SameLine();
    }
    if (!nodePresent)
    {
        ImGui::Text(nameNoId);
        if (desc->Description != "") //Draw tooltip if not empty
            gui::TooltipOnPrevious(desc->Description, nullptr);

        return;
    }
    string name = nameNoId + fmt::format("##{}", (u64)node.get());

    //Todo: See if this can be done by implementing some IXtblNode abstract interface for each XtblType value. Would likely be much cleaner code
    //Draw node
    f32 nodeWidth = 240.0f;
    ImGui::PushItemWidth(nodeWidth);
    switch (desc->Type)
    {
    case XtblType::String:
        ImGui::InputText(name, std::get<string>(node->Value));
        break;
    case XtblType::Int:
        if(desc->Min && desc->Max)
            ImGui::SliderInt(name.c_str(), &std::get<i32>(node->Value), (i32)desc->Min.value(), (i32)desc->Max.value());
        else
            ImGui::InputInt(name.c_str(), &std::get<i32>(node->Value));

        break;
    case XtblType::Float:
        if (desc->Min && desc->Max)
            ImGui::SliderFloat(name.c_str(), &std::get<f32>(node->Value), desc->Min.value(), desc->Max.value());
        else
            ImGui::InputFloat(name.c_str(), &std::get<f32>(node->Value));
        
        break;
    case XtblType::Vector:
        ImGui::InputFloat3(name.c_str(), (f32*)&std::get<Vec3>(node->Value));
        break;
    case XtblType::Color:
        ImGui::ColorPicker3(name.c_str(), (f32*)&std::get<Vec3>(node->Value));
        break;
    case XtblType::Selection:
        if (ImGui::BeginCombo(name.c_str(), std::get<string>(node->Value).c_str()))
        {
            for (auto& choice : desc->Choices)
            {
                bool selected = choice == std::get<string>(node->Value);
                if (ImGui::Selectable(choice.c_str(), &selected))
                    node->Value = choice;
            }
            ImGui::EndCombo();
        }
        break;
    case XtblType::Filename:
        //Todo: Get a list of files with correct format for this node and list those instead of having the player type names out
        ImGui::InputText(name, std::get<string>(node->Value)); 
        break;
    case XtblType::ComboElement:
        //Todo: Add support for this type. It's like a c/c++ union
        break;
    case XtblType::Grid: //Table of values
        {
            ImGui::Text("%s:", node->Name.c_str());
            if (ImGui::BeginTable(name.c_str(), desc->Subnodes[0]->Subnodes.size(), ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_BordersOuter 
                | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable))
            {
                //Setup columns
                ImGui::TableSetupScrollFreeze(0, 1);
                for (auto& subdesc : desc->Subnodes[0]->Subnodes)
                    ImGui::TableSetupColumn(subdesc->Name.c_str(), ImGuiTableColumnFlags_None, nodeWidth * 1.14f);
                
                //Fill table data
                ImGui::TableHeadersRow();
                for (auto& subnode : node->Subnodes)
                {
                    ImGui::TableNextRow();
                    for (auto& subdesc : desc->Subnodes[0]->Subnodes)
                    {
                        ImGui::TableNextColumn();
                        DrawXtblEntry(subdesc, "", subnode->GetSubnode(subdesc->Name));
                    }
                }

                ImGui::EndTable();
            }
        }
        break;
    case XtblType::Reference: //Variable that references another xtbl
        {
            //Get referenced xtbl
            auto refXtbl = xtblManager_->GetOrCreateXtbl(xtbl_->VppName, desc->Reference->File);
            auto split = String::SplitString(desc->Reference->Path, "/");
            string variablePath = desc->Reference->Path.substr(desc->Reference->Path.find_first_of('/') + 1);

            //Fill combo with valid values for node
            if (refXtbl && ImGui::BeginCombo(name.c_str(), std::get<string>(node->Value).c_str()))
            {
                //Find subnodes in referenced xtbl which match reference path
                for (auto& subnode : refXtbl->Entries)
                {
                    if (!String::EqualIgnoreCase(subnode->Name, split[0]))
                        continue;

                    //Get list of matching subnodes. Some files like human_team_names.xtbl use lists instead of separate elements
                    auto variables = refXtbl->GetSubnodes(variablePath, subnode);
                    if (variables.size() == 0)
                        continue;

                    //Add combo option for each variable
                    for (auto& variable : variables)
                    {
                        if (variable->Type == XtblType::String)
                        {
                            string variableValue = std::get<string>(variable->Value);
                            string& nodeValue = std::get<string>(node->Value);
                            bool selected = variableValue == nodeValue;
                            if (ImGui::Selectable(variableValue.c_str(), &selected))
                                nodeValue = variableValue;
                        }
                        else
                        {
                            gui::LabelAndValue(nameNoId + ":", std::get<string>(node->Value) + " (reference) | Unsupported XtblType: " + to_string(variable->Type));
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }
        break;
    case XtblType::Flag: //Note: This shouldn't ever be reached since the XtblType::Flags case should handle this
        gui::LabelAndValue(nameNoId + ":", std::get<string>(node->Value));
        break;
    case XtblType::Flags:
        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc->Description != "")
                gui::TooltipOnPrevious(desc->Description, nullptr);
            for (auto& subnode : node->Subnodes)
            {
                auto& value = std::get<XtblFlag>(subnode->Value);
                ImGui::Checkbox(value.Name.c_str(), &value.Value);
            }

            ImGui::TreePop();
        }
        break;
    case XtblType::List:
        //Draw subnodes
        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc->Description != "")
                gui::TooltipOnPrevious(desc->Description, nullptr);
            for (auto& subnode : node->Subnodes)
            {
                //auto& subnode = node->Subnodes[0];
                //Try to get <Name></Name> value
                string subnodeName = subnode->Name;
                auto nameValue = subnode->GetSubnodeValueString("Name");
                if (nameValue)
                    subnodeName = nameValue.value();

                //Gets parent node name and current node name in path
                string subdescPath = subnode->GetPath();
                subdescPath = subdescPath.substr(String::FindNthCharacterFromBack(subdescPath, '/', 2) + 1);
                Handle<XtblDescription> subdesc = xtbl_->GetValueDescription(subdescPath, desc);
                if (subdesc)
                    DrawXtblEntry(subdesc, subnodeName.c_str(), subnode);
            }

            ImGui::TreePop();
        }
        break;
    case XtblType::Element:
        //Draw subnodes
        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc->Description != "")
                gui::TooltipOnPrevious(desc->Description, nullptr);
            for (auto& subnode : node->Subnodes)
            {
                //Gets parent node name and current node name in path
                string subdescPath = subnode->GetPath();
                subdescPath = subdescPath.substr(String::FindNthCharacterFromBack(subdescPath, '/', 2) + 1);
                Handle<XtblDescription> subdesc = xtbl_->GetValueDescription(subdescPath, desc);
                if (subdesc)
                    DrawXtblEntry(subdesc, subdesc->DisplayName.c_str(), subnode);
            }

            ImGui::TreePop();
        }
        break;
    case XtblType::None:
    case XtblType::TableDescription:
    default:
        if (node->HasSubnodes())
        {
            if (ImGui::TreeNode(name.c_str()))
            {
                if (desc->Description != "")
                    gui::TooltipOnPrevious(desc->Description, nullptr);
                for (auto& subnode : node->Subnodes)
                {
                //Gets parent node name and current node name in path
                string subdescPath = subnode->GetPath();
                subdescPath = subdescPath.substr(String::FindNthCharacterFromBack(subdescPath, '/', 2) + 1);
                Handle<XtblDescription> subdesc = xtbl_->GetValueDescription(subdescPath, desc);
                    if (subdesc)
                        DrawXtblEntry(subdesc, subdesc->DisplayName.c_str(), subnode);
                }

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
    if (desc->Description != "")
        gui::TooltipOnPrevious(desc->Description, nullptr);

    ImGui::PopItemWidth();
}