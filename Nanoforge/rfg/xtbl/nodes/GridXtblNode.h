#pragma once
#include "rfg/xtbl/XtblNodes.h"
#include "Common/Typedefs.h"
#include "XtblDescription.h"
#include "IXtblNode.h"
#include "XtblType.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"
#include <unordered_map>

//Node which is a table of values with one or more rows and columns
class GridXtblNode : public IXtblNode
{
public:
#pragma warning(disable:4100) //Disable warning about unused argument. Can't remove the arg since some implementations of this function use it.
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr)
    {
        const f32 indent = 30.0f;
        ImGui::Separator();
        ImGui::Indent(indent);

        CalculateEditorValues(xtbl, nameOverride);
        bool editedThisFrame = false; //Used for document unsaved change tracking
        ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter
            | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable
            | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;

        //Get column data
        const bool hasSingleColumn = desc_->Subnodes[0]->Subnodes.size() == 0;
        auto& columnDescs = hasSingleColumn ? desc_->Subnodes : desc_->Subnodes[0]->Subnodes;

        //Separate child elements into ones that can be stored in the table, and ones which can't
        //Grid/table child elements can't as it becomes unreadablel
        std::vector<Handle<XtblDescription>> tableDescs = {};
        std::vector<Handle<XtblDescription>> nonTableDescs = {};
        for (u32 i = 0; i < columnDescs.size(); i++)
        {
            Handle<XtblDescription> desc = columnDescs[i];
            if (desc->Type == XtblType::Grid)
                nonTableDescs.push_back(desc);
            else
                tableDescs.push_back(desc);
        }

        //Todo: Try wrapping this in a child window so it's clear the table and other items are linked
        //Draw child elements that can fit in the table
        i32 selectedIndex = 0;
        const f32 tableWidth = std::min(500.0f, ImGui::GetWindowWidth());
        ImGui::Text("%s:", nameNoId_.value().c_str());
        if (ImGui::BeginTable(name_.value().c_str(), (int)tableDescs.size(), flags))
        {
            //Setup columns
            ImGui::TableSetupScrollFreeze(0, 1);
            for (Handle<XtblDescription> subdesc : tableDescs)
                ImGui::TableSetupColumn(subdesc->DisplayName.c_str(), ImGuiTableColumnFlags_None);

            //Fill table data
            ImGui::TableHeadersRow();
            for (u32 i = 0; i < Subnodes.size(); i++) //Loop through rows
            {
                IXtblNode* subnode = Subnodes[i];
                ImGui::TableNextRow();
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) //If row is clicked, show it's elements that can't fit in the table below it
                    selectedIndex = i;

                for (Handle<XtblDescription> subdesc : tableDescs) //Loop through columns
                {
                    ImGui::TableNextColumn();

                    //Draw row data with empty name since the name is already in the column header
                    ImGui::PushItemWidth(NodeGuiWidth);
                    auto nodeOverride = hasSingleColumn ? subnode : subnode->GetSubnode(subdesc->Name);
                    if (DrawNodeByDescription(guiState, xtbl, subdesc, subnode, "", nodeOverride))
                        editedThisFrame = true;

                    ImGui::PopItemWidth();
                }
            }

            ImGui::EndTable();
        }

        //Draw child elements that can't fit in the table
        ImGui::Indent(indent);
        for (u32 i = 0; i < Subnodes.size(); i++)
        {
            if (i != selectedIndex) //Only draw the child elements of the currently selected row in the table
                continue;

            IXtblNode* subnode = Subnodes[i];
            for (Handle<XtblDescription> subdesc : nonTableDescs)
            {
                if (DrawNodeByDescription(guiState, xtbl, subdesc, subnode, nullptr, subnode->GetSubnode(subdesc->Name)))
                    editedThisFrame = true;
            }
        }
        ImGui::Unindent(indent);

        ImGui::Unindent(indent);
        ImGui::Separator();

        return editedThisFrame;
    }
#pragma warning(default: 4100)

    virtual void InitDefault()
    {

    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        //Make another xml element and write edited subnodes to it
        auto* gridXml = xml->InsertNewChildElement(Name.c_str());
        for (auto& subnode : Subnodes)
        {
            if (!subnode->Edited)
                continue;

            //Stop early if any node fails to write
            bool result = subnode->WriteModinfoEdits(gridXml);
            if (!result)
            {
                LOG_ERROR("Failed to write modinfo data for xtbl node \"{}\"", subnode->GetPath());
                return false;
            }
        }

        return true;
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        //Make another xml element and write edited subnodes to it
        auto* gridXml = xml->InsertNewChildElement(Name.c_str());
        for (auto& subnode : Subnodes)
        {
            //Stop early if any node fails to write
            bool result = subnode->WriteXml(gridXml, writeNanoforgeMetadata);
            if (!result)
            {
                LOG_ERROR("Failed to write modinfo data for xtbl node \"{}\"", subnode->GetPath());
                return false;
            }
        }

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            gridXml->SetAttribute("__NanoforgeEdited", Edited);
            gridXml->SetAttribute("__NanoforgeNewEntry", NewEntry);
        }

        return true;
    }
};