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
#include <tinyxml2/tinyxml2.h>
#include <vector>
#include <optional>
#include <variant>

/*This file has all the IXtblNode implementations. One implementation per XtblType enum value.*/
static f32 NodeGuiWidth = 240.0f;

class ElementXtblNode : public IXtblNode
{
public:
    ~ElementXtblNode()
    {
    
    }
    
    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
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
                subnode->DrawEditor(xtblManager, xtbl);
            }

            ImGui::TreePop();
        }
    }
    
    virtual void InitDefault()
    {
    
    }
};

class StringXtblNode : public IXtblNode
{
public:
    ~StringXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
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

class IntXtblNode : public IXtblNode
{
public:
    ~IntXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
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

class FloatXtblNode : public IXtblNode
{
public:
    ~FloatXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
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

class VectorXtblNode : public IXtblNode
{
public:
    ~VectorXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
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

class ColorXtblNode : public IXtblNode
{
public:
    ~ColorXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
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

class SelectionXtblNode : public IXtblNode
{
public:
    ~SelectionXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
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

class FlagsXtblNode : public IXtblNode
{
public:
    ~FlagsXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
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

class ListXtblNode : public IXtblNode
{
public:
    ~ListXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
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

                subnode->DrawEditor(xtblManager, xtbl, subnodeName.c_str());
            }

            ImGui::TreePop();
        }
    }

    virtual void InitDefault()
    {

    }
};

class FilenameXtblNode : public IXtblNode
{
public:
    ~FilenameXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
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

class ComboElementXtblNode : public IXtblNode
{
public:
    ~ComboElementXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        //Todo: Add support for this type. It's like a c/c++ union
    }

    virtual void InitDefault()
    {

    }
};

class ReferenceXtblNode : public IXtblNode
{
public:
    ~ReferenceXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        //Get referenced xtbl
        auto refXtbl = xtblManager->GetOrCreateXtbl(xtbl->VppName, desc->Reference->File);
        auto split = String::SplitString(desc->Reference->Path, "/");
        string variablePath = desc->Reference->Path.substr(desc->Reference->Path.find_first_of('/') + 1);

        //Fill combo with valid values for node
        if (refXtbl && ImGui::BeginCombo(name.c_str(), std::get<string>(Value).c_str()))
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
                        string& nodeValue = std::get<string>(Value);
                        bool selected = variableValue == nodeValue;
                        if (ImGui::Selectable(variableValue.c_str(), &selected))
                            nodeValue = variableValue;
                    }
                    else
                    {
                        gui::LabelAndValue(nameNoId + ":", std::get<string>(Value) + " (reference) | Unsupported XtblType: " + to_string(variable->Type));
                    }
                }
            }
            ImGui::EndCombo();
        }
    }

    virtual void InitDefault()
    {

    }
};

class GridXtblNode : public IXtblNode
{
public:
    ~GridXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
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
                    auto subnode2 = subnode->GetSubnode(subdesc->Name);
                    if(subnode2)
                        subnode2->DrawEditor(xtblManager, xtbl);
                }
            }

            ImGui::EndTable();
        }
    }

    virtual void InitDefault()
    {

    }
};

class TableDescriptionXtblNode : public IXtblNode
{
public:
    ~TableDescriptionXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
    {
        
    }

    virtual void InitDefault()
    {

    }
};

class FlagXtblNode : public IXtblNode
{
public:
    ~FlagXtblNode()
    {

    }

    virtual void DrawEditor(XtblManager* xtblManager, Handle<XtblFile> xtbl, const char* nameOverride = nullptr)
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
static Handle<IXtblNode> CreateDefaultNode(Handle<XtblDescription> desc)
{
    Handle<IXtblNode> node = nullptr;
    switch (desc->Type)
    {
    case XtblType::Element:
        node = CreateHandle<ElementXtblNode>();
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

    return node;
}