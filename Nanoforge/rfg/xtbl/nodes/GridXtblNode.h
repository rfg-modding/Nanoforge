#pragma once
#include "rfg/xtbl/XtblNodes.h"
#include "Common/Typedefs.h"
#include "XtblDescription.h"
#include "IXtblNode.h"
#include "XtblType.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"

//Node which is a table of values with one or more rows and columns
class GridXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr)
    {
        CalculateEditorValues(xtbl, nameOverride);
        bool editedThisFrame = false; //Used for document unsaved change tracking
        ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter
            | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable
            | ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingStretchSame;

        //Get column data
        const bool hasSingleColumn = desc_->Subnodes[0]->Subnodes.size() == 0;
        auto& columnDescs = hasSingleColumn ? desc_->Subnodes : desc_->Subnodes[0]->Subnodes;

        ImGui::Text("%s:", nameNoId_.value().c_str());
        if (ImGui::BeginTable(name_.value().c_str(), columnDescs.size(), flags))
        {
            //Setup columns
            ImGui::TableSetupScrollFreeze(0, 1);
            for (auto& subdesc : columnDescs)
                ImGui::TableSetupColumn(subdesc->DisplayName.c_str(), ImGuiTableColumnFlags_None);

            //Fill table data
            ImGui::TableHeadersRow();
            for (auto& subnode : Subnodes)
            {
                ImGui::TableNextRow();
                for (auto& subdesc : columnDescs)
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
        ImGui::Separator();

        return editedThisFrame;
    }

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