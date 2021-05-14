#pragma once
#include "Common/Typedefs.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"
#include "XtblType.h"
#include "IXtblNode.h"
#include "XtblDescription.h"
#include "XtblManager.h"
#include "RfgTools++/types/Vec3.h"
#include "RfgTools++/types/Vec4.h"
#include "gui/GuiState.h"
#include "gui/documents/XtblDocument.h"
#include <tinyxml2/tinyxml2.h>
#include <vector>
#include <optional>
#include <variant>

/*This file has all the IXtblNode implementations. One implementation per XtblType enum value.*/
static f32 NodeGuiWidth = 240.0f;

//Create node from XtblType using default value for that type
static Handle<IXtblNode> CreateDefaultNode(XtblType type);
static Handle<IXtblNode> CreateDefaultNode(Handle<XtblDescription> desc, bool addSubnodes = false);

//Use descriptions to draw nodes so non-existant optional nodes are drawn
//Also handles drawing description tooltips and checkboxes for optional nodes
static void DrawNodeByDescription(GuiState* guiState, Handle<XtblFile> xtbl, Handle<XtblDescription> desc, Handle<IXtblNode> rootNode, 
                                  const char* nameOverride = nullptr, Handle<IXtblNode> nodeOverride = nullptr)
{
    //Use lambda to get node since we need this logic multiple times
    string descPath = desc->GetPath();
    auto getNode = [&]() -> Handle<IXtblNode>
    {
        return nodeOverride ? nodeOverride : xtbl->GetSubnode(descPath.substr(descPath.find_first_of('/') + 1), rootNode);
    };
    Handle<IXtblNode> node = getNode();
    bool nodePresent = (node != nullptr && node->Enabled);

    //IDs used by dear imgui elements
    string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
    string checkboxId = fmt::format("##{}", (u64)desc.get()); //Using pointer address as ID since it's unique

    //Draw checkbox for optional elements
    if (!desc->Required)
    {
        if (ImGui::Checkbox(checkboxId.c_str(), &nodePresent))
        {
            //When checkbox is set to true ensure elements exist in node
            if (nodePresent)
            {
                xtbl->EnsureEntryExists(desc, rootNode);
                node = getNode();
                node->Enabled = nodePresent;
                return;
            }
            else
            {
                node->Enabled = false;
            }
        }
        ImGui::SameLine();
    }
    
    //If an optional element isn't present draw it's display name
    if (!nodePresent)
    {
        ImGui::Text(nameNoId);
        if (desc->Description != "") //Draw tooltip if not empty
            gui::TooltipOnPrevious(desc->Description, nullptr);

        return;
    }

    //If a node is present draw it.
    if(nodePresent)
        node->DrawEditor(guiState, xtbl, rootNode, nameOverride);

    //Draw tooltip if not empty
    if (desc->Description != "")
        gui::TooltipOnPrevious(desc->Description, nullptr);
}

//A node that contains many other nodes. Usually the root node of an entry is an element node. E.g. each <Vehicle> block is vehicles.xtbl is an element node.
class ElementXtblNode : public IXtblNode
{
public:
    ~ElementXtblNode()
    {
    
    }
    
    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        //Draw subnodes
        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc->Description != "")
                gui::TooltipOnPrevious(desc->Description, nullptr);
            for (auto& subnode : Subnodes)
            {
                //Gets parent node name and current node name in path
                string subdescPath = subnode->GetPath();
                subdescPath = subdescPath.substr(String::FindNthCharacterFromBack(subdescPath, '/', 2) + 1);
                Handle<XtblDescription> subdesc = xtbl->GetValueDescription(subdescPath, desc);
                if (subdesc)
                    DrawNodeByDescription(guiState, xtbl, subdesc, rootNode, subdesc->DisplayName.c_str());
            }

            ImGui::TreePop();
        }
    }
    
    virtual void InitDefault()
    {
    
    }
};

//Node with a string value
class StringXtblNode : public IXtblNode
{
public:
    ~StringXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        ImGui::InputText(name, std::get<string>(Value));
    }

    virtual void InitDefault()
    {
        Value = "";
    }
};

//Node with a signed integer value
class IntXtblNode : public IXtblNode
{
public:
    ~IntXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        if (desc->Min && desc->Max)
            ImGui::SliderInt(name.c_str(), &std::get<i32>(Value), (i32)desc->Min.value(), (i32)desc->Max.value());
        else
            ImGui::InputInt(name.c_str(), &std::get<i32>(Value));
    }

    virtual void InitDefault()
    {
        Value = 0;
    }
};

//Node with a floating point value
class FloatXtblNode : public IXtblNode
{
public:
    ~FloatXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        if (desc->Min && desc->Max)
            ImGui::SliderFloat(name.c_str(), &std::get<f32>(Value), desc->Min.value(), desc->Max.value());
        else
            ImGui::InputFloat(name.c_str(), &std::get<f32>(Value));
    }

    virtual void InitDefault()
    {
        Value = 0.0f;
    }
};

//Node with a 3d vector value
class VectorXtblNode : public IXtblNode
{
public:
    ~VectorXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        ImGui::InputFloat3(name.c_str(), (f32*)&std::get<Vec3>(Value));
    }

    virtual void InitDefault()
    {
        Value = Vec3(0.0f, 0.0f, 0.0f);
    }
};

//Node with a RGB color value
class ColorXtblNode : public IXtblNode
{
public:
    ~ColorXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        ImGui::ColorPicker3(name.c_str(), (f32*)&std::get<Vec3>(Value));
    }

    virtual void InitDefault()
    {
        Value = Vec3(0.0f, 0.0f, 0.0f);
    }
};

//Node with preset options described in xtbl description block
class SelectionXtblNode : public IXtblNode
{
public:
    ~SelectionXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        if (ImGui::BeginCombo(name.c_str(), std::get<string>(Value).c_str()))
        {
            for (auto& choice : desc->Choices)
            {
                bool selected = choice == std::get<string>(Value);
                if (ImGui::Selectable(choice.c_str(), &selected))
                    Value = choice;
            }
            ImGui::EndCombo();
        }
    }

    virtual void InitDefault()
    {
        Value = "";
    }
};

//Node with set of flags that can be true or false. Flags are described in xtbl description block
class FlagsXtblNode : public IXtblNode
{
public:
    ~FlagsXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc->Description != "")
                gui::TooltipOnPrevious(desc->Description, nullptr);
            for (auto& subnode : Subnodes)
            {
                auto& value = std::get<XtblFlag>(subnode->Value);
                ImGui::Checkbox(value.Name.c_str(), &value.Value);
            }

            ImGui::TreePop();
        }
    }

    virtual void InitDefault()
    {

    }
};

//Node containing a list of other nodes
class ListXtblNode : public IXtblNode
{
public:
    ~ListXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        //Draw subnodes
        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc->Description != "")
                gui::TooltipOnPrevious(desc->Description, nullptr);

            for (auto& subnode : Subnodes)
            {
                //Try to get <Name></Name> value
                string subnodeName = subnode->Name;
                auto nameValue = subnode->GetSubnodeValueString("Name");
                if (nameValue)
                    subnodeName = nameValue.value();

                //Gets parent node name and current node name in path
                string subdescPath = subnode->GetPath();
                subdescPath = subdescPath.substr(String::FindNthCharacterFromBack(subdescPath, '/', 2) + 1);
                Handle<XtblDescription> subdesc = xtbl->GetValueDescription(subdescPath, desc);
                if (subdesc)
                    DrawNodeByDescription(guiState, xtbl, subdesc, rootNode, subnodeName.c_str(), subnode);
            }

            ImGui::TreePop();
        }
    }

    virtual void InitDefault()
    {

    }
};

//Node with the name of an RFG asset file
class FilenameXtblNode : public IXtblNode
{
public:
    ~FilenameXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        //Todo: Get a list of files with correct format for this node and list those instead of having the player type names out
        ImGui::InputText(name, std::get<string>(Value));
    }

    virtual void InitDefault()
    {
        Value = "";
    }
};

//Node that can have multiple value types. Similar to a union in C/C++
class ComboElementXtblNode : public IXtblNode
{
public:
    ~ComboElementXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        //This type of xtbl node acts like a C/C++ Union, so multiple types can be chosen
        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc->Description != "")
                gui::TooltipOnPrevious(desc->Description, nullptr);

            //Get current type. Only one type from the desc is present
            u32 currentType = 0xFFFFFFFF;
            for (u32 i = 0; i < desc->Subnodes.size(); i++)
                for (auto& subnode : Subnodes)
                    if (String::EqualIgnoreCase(desc->Subnodes[i]->Name, subnode->Name))
                        currentType = i;

            //Draw a radio button for each type
            for (u32 i = 0; i < desc->Subnodes.size(); i++)
            {
                bool active = i == currentType;
                if (ImGui::RadioButton(desc->Subnodes[i]->Name.c_str(), active))
                {
                    //If different type is selected activate new node and delete current one
                    if (!active)
                    {
                        //Delete current nodes
                        for (auto& subnode : Subnodes)
                        {
                            subnode->DeleteSubnodes();
                            subnode->Parent = nullptr;
                        }
                        Subnodes.clear();

                        //Create default node
                        auto subdesc = desc->Subnodes[i];
                        auto subnode = CreateDefaultNode(subdesc, true);
                        subnode->Name = subdesc->Name;
                        subnode->Type = subdesc->Type;
                        subnode->Parent = Parent->GetSubnode(Name);
                        subnode->CategorySet = false;
                        subnode->HasDescription = true;
                        subnode->Enabled = true;
                        Subnodes.push_back(subnode);
                    }
                }
                if (i != desc->Subnodes.size() - 1)
                    ImGui::SameLine();
            }

            //Draw current type
            for (auto& subnode : Subnodes)
            {
                //Gets parent node name and current node name in path
                string subdescPath = subnode->GetPath();
                subdescPath = subdescPath.substr(String::FindNthCharacterFromBack(subdescPath, '/', 2) + 1);
                Handle<XtblDescription> subdesc = xtbl->GetValueDescription(subdescPath, desc);
                if (subdesc)
                    DrawNodeByDescription(guiState, xtbl, subdesc, rootNode, subdesc->DisplayName.c_str());
            }

            ImGui::TreePop();
        }
    }

    virtual void InitDefault()
    {

    }
};

//Node which references the value of nodes in another xtbl
class ReferenceXtblNode : public IXtblNode
{
public:
    ~ReferenceXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        //Get referenced xtbl
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);
        auto refXtbl = guiState->Xtbls->GetOrCreateXtbl(xtbl->VppName, desc->Reference->File);
        if (!refXtbl)
            return;

        //Find referenced values
        std::vector<Handle<IXtblNode>> referencedNodes = {};
        auto split = String::SplitString(desc->Reference->Path, "/");
        string variablePath = desc->Reference->Path.substr(desc->Reference->Path.find_first_of('/') + 1);
        for (auto& subnode : refXtbl->Entries)
        {
            if (!String::EqualIgnoreCase(subnode->Name, split[0]))
                continue;

            //Get list of matching subnodes. Some files like human_team_names.xtbl use lists instead of separate elements
            auto variables = refXtbl->GetSubnodes(variablePath, subnode);
            if (variables.size() == 0)
                continue;

            for (auto& variable : variables)
                referencedNodes.push_back(variable);
        }

        //Find largest reference name to align combo buttons
        ImVec2 maxReferenceSize = { 0.0f, 0.0f };
        for (auto& node : referencedNodes)
        {
            ImVec2 size = ImGui::CalcTextSize(std::get<string>(node->Value).c_str());
            if (maxReferenceSize.x < size.x)
                maxReferenceSize.x = size.x;
            if (maxReferenceSize.y < size.y)
                maxReferenceSize.y = size.y;
        }

        //Offset used to make combo buttons line up with text
        static f32 comboButtonWidth = 26.5f;
        static f32 comboButtonHeight = 25.0f;
        static f32 comboButtonOffsetY = -5.0f;

        //Draw combo with an option for each referenced value
        if (ImGui::BeginCombo(name.c_str(), std::get<string>(Value).c_str()))
        {
            for (auto& node : referencedNodes)
            {
                if (node->Type == XtblType::String)
                {
                    //Draw node name selectable
                    string variableValue = std::get<string>(node->Value);
                    string& nodeValue = std::get<string>(Value);
                    bool selected = variableValue == nodeValue;
                    if (ImGui::Selectable(variableValue.c_str(), &selected, 0, maxReferenceSize))
                        nodeValue = variableValue;

                    //Draw button to jump to file being referenced
                    ImGui::SameLine();
                    ImGui::SetCursorPos({ ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + comboButtonOffsetY });
                    if (ImGui::Button(fmt::format("{}##{}", ICON_FA_EXTERNAL_LINK_ALT, (u64)node.get()).c_str(), ImVec2(comboButtonWidth, comboButtonHeight)))
                    {
                        //Find parent element of variable
                        Handle<IXtblNode> documentRoot = node;
                        while (String::Contains(documentRoot->GetPath(), "/") && documentRoot->Parent)
                        {
                            documentRoot = documentRoot->Parent;
                        }

                        //Jump to referenced xtbl and select current node
                        auto document = std::dynamic_pointer_cast<XtblDocument>(guiState->GetDocument(refXtbl->Name));
                        if (document)
                        {
                            document->SelectedNode = documentRoot;
                            ImGui::SetWindowFocus(document->Title.c_str());
                        }
                        else
                        {
                            document = CreateHandle<XtblDocument>(guiState, refXtbl->Name, refXtbl->VppName, refXtbl->VppName, false, documentRoot);
                            guiState->CreateDocument(refXtbl->Name, document);
                        }
                    }
                }
                else
                {
                    gui::LabelAndValue(nameNoId + ":", std::get<string>(Value) + " (reference) | Unsupported XtblType: " + to_string(node->Type));
                }
            }

            ImGui::EndCombo();
        }
    }

    virtual void InitDefault()
    {

    }
};

//Node which is a table of values with one or more rows and columns
class GridXtblNode : public IXtblNode
{
public:
    ~GridXtblNode()
    {

    }

    //Todo: Optimize this. Some grids cause serious performance issues. E.g. Open player.xtbl and scroll down to the bottom of the grid
    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_BordersOuter
                                | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable 
                                | ImGuiTableFlags_Hideable;

        ImGui::Text("%s:", nameNoId.c_str());
        if (ImGui::BeginTable(name.c_str(), desc->Subnodes[0]->Subnodes.size(), flags))
        {
            //Setup columns
            ImGui::TableSetupScrollFreeze(0, 1);
            for (auto& subdesc : desc->Subnodes[0]->Subnodes)
                ImGui::TableSetupColumn(subdesc->Name.c_str(), ImGuiTableColumnFlags_None, NodeGuiWidth * 1.14f);

            //Fill table data
            ImGui::TableHeadersRow();
            for (auto& subnode : Subnodes)
            {
                ImGui::TableNextRow();
                for (auto& subdesc : desc->Subnodes[0]->Subnodes)
                {
                    ImGui::TableNextColumn();
                    DrawNodeByDescription(guiState, xtbl, subdesc, rootNode, nullptr, subnode->GetSubnode(subdesc->Name));
                }
            }

            ImGui::EndTable();
        }
    }

    virtual void InitDefault()
    {

    }
};

//Node found in xtbl description block found at the end of xtbl files
class TableDescriptionXtblNode : public IXtblNode
{
public:
    ~TableDescriptionXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        
    }

    virtual void InitDefault()
    {

    }
};

//Flag node. Contained by FlagsXtblNode
class FlagXtblNode : public IXtblNode
{
public:
    ~FlagXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        
    }

    virtual void InitDefault()
    {

    }
};

//Create node from XtblType using default value for that type
static Handle<IXtblNode> CreateDefaultNode(XtblType type)
{
    switch (type)
    {
    case XtblType::Element:
        return CreateHandle<ElementXtblNode>();
    case XtblType::String:
        return CreateHandle<StringXtblNode>();
    case XtblType::Int:
        return CreateHandle<IntXtblNode>();
    case XtblType::Float:
        return CreateHandle<FloatXtblNode>();
    case XtblType::Vector:
        return CreateHandle<VectorXtblNode>();
    case XtblType::Color:
        return CreateHandle<ColorXtblNode>();
    case XtblType::Selection:
        return CreateHandle<SelectionXtblNode>();
    case XtblType::Flags:
        return CreateHandle<FlagsXtblNode>();
    case XtblType::List:
        return CreateHandle<ListXtblNode>();
    case XtblType::Filename:
        return CreateHandle<FilenameXtblNode>();
    case XtblType::ComboElement:
        return CreateHandle<ComboElementXtblNode>();
    case XtblType::Reference:
        return CreateHandle<ReferenceXtblNode>();
    case XtblType::Grid:
        return CreateHandle<GridXtblNode>();
    case XtblType::TableDescription:
        return CreateHandle<TableDescriptionXtblNode>();
    case XtblType::Flag:
        return CreateHandle<FlagXtblNode>();
    case XtblType::None:
    default:
        THROW_EXCEPTION("Invalid value for XtblType \"{}\" passed to CreateDefaultNode() in XtblNodes.h", type);
    }
}

//Create node from XtblDescription using default value from that description entry
static Handle<IXtblNode> CreateDefaultNode(Handle<XtblDescription> desc, bool addSubnodes)
{
    Handle<IXtblNode> node = nullptr;
    switch (desc->Type)
    {
    case XtblType::Element:
        node = CreateHandle<ElementXtblNode>();
        if (addSubnodes)
        {
            for (auto& subdesc : desc->Subnodes)
            {
                auto subnode = CreateDefaultNode(subdesc);
                if (subnode)
                {
                    subnode->Parent = node;
                    node->Subnodes.push_back(subnode);
                }
            }
        }
        break;
    case XtblType::String:
        node = CreateHandle<StringXtblNode>();
        node->Value = desc->Default;
        break;
    case XtblType::Int:
        node = CreateHandle<IntXtblNode>();
        node->Value = std::stoi(desc->Default);
        break;
    case XtblType::Float:
        node = CreateHandle<FloatXtblNode>();
        node->Value = std::stof(desc->Default);
        break;
    case XtblType::Vector:
        node = CreateHandle<VectorXtblNode>();
        break;
    case XtblType::Color:
        node = CreateHandle<ColorXtblNode>();
        break;
    case XtblType::Selection:
        node = CreateHandle<SelectionXtblNode>();
        node->Value = desc->Default;
        break;
    case XtblType::Flags:
        node = CreateHandle<FlagsXtblNode>();
        //Ensure there's a subnode for every flag listed in <TableDescription>
        for (auto& flagDesc : desc->Flags)
        {
            //Check if flag has a subnode
            Handle<IXtblNode> flagNode = nullptr;
            for (auto& flag : node->Subnodes)
            {
                auto value = std::get<XtblFlag>(flag->Value);
                if (value.Name == flagDesc)
                {
                    flagNode = flag;
                    break;
                }
            }

            //If flag isn't present create a subnode for it
            if (!flagNode)
            {
                auto subnode = CreateDefaultNode(XtblType::Flag);
                subnode->Name = "Flag";
                subnode->Type = XtblType::Flag;
                subnode->Parent = node;
                subnode->CategorySet = false;
                subnode->HasDescription = true;
                subnode->Enabled = true;
                subnode->Value = XtblFlag{ .Name = flagDesc, .Value = false };
                node->Subnodes.push_back(subnode);
            }
        }
        break;
    case XtblType::List:
        node = CreateHandle<ListXtblNode>();
        break;
    case XtblType::Filename:
        node = CreateHandle<FilenameXtblNode>();
        node->Value = desc->Default;
        break;
    case XtblType::ComboElement:
        node = CreateHandle<ComboElementXtblNode>();
        break;
    case XtblType::Reference:
        node = CreateHandle<ReferenceXtblNode>();
        node->Value = desc->Default;
        break;
    case XtblType::Grid:
        node = CreateHandle<GridXtblNode>();
        break;
    case XtblType::TableDescription:
        node = CreateHandle<TableDescriptionXtblNode>();
        break;
    case XtblType::Flag:
        node = CreateHandle<FlagXtblNode>();
        break;
    case XtblType::None:
    default:
        THROW_EXCEPTION("Invalid value for XtblType \"{}\" passed to CreateDefaultNode() in XtblNodes.h", desc->Type);
    }

    node->Name = desc->Name;
    node->Type = desc->Type;
    node->HasDescription = true;
    return node;
}