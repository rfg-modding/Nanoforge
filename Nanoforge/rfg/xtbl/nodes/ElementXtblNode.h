#pragma once
#include "rfg/xtbl/XtblNodes.h"
#include "Common/Typedefs.h"
#include "XtblDescription.h"
#include "IXtblNode.h"
#include "XtblType.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"

//A node that contains many other nodes. Usually the root node of an stringXml is an element node. E.g. each <Vehicle> block is vehicles.xtbl is an element node.
class ElementXtblNode : public IXtblNode
{
public:
    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr)
    {
        CalculateEditorValues(xtbl, nameOverride);
        auto drawTooltip = [&]()
        {
            if (desc_->Description.has_value() && desc_->Description != "")
                gui::TooltipOnPrevious(desc_->Description.value(), nullptr);
        };
        auto drawNode = [&]()
        {
            for (auto& subdesc : desc_->Subnodes)
                DrawNodeByDescription(guiState, xtbl, subdesc, this, nullptr, GetSubnode(subdesc->Name));
        };

        //Draw subnodes
        if (parent->Type == XtblType::List)
        {
            //Draw the subnodes directly when the parent is a list since lists already put each item in a tree node
            drawTooltip();
            drawNode();
        }
        else
        {
            //In all other cases make a tree node and draw subnodes within it
            if (ImGui::TreeNode(name_.value().c_str()))
            {
                drawTooltip();
                drawNode();
                ImGui::TreePop();
            }
        }
    }

    virtual void InitDefault()
    {

    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        //Make another xml element and write edited subnodes to it
        auto* elementXml = xml->InsertNewChildElement(Name.c_str());
        for (auto& subnode : Subnodes)
        {
            if (!subnode->Edited)
                continue;

            //Stop early if any node fails to write
            bool result = subnode->WriteModinfoEdits(elementXml);
            if (!result)
            {
                Log->error("Failed to write modinfo data for xtbl node \"{}\"", subnode->GetPath());
                return false;
            }
        }

        return true;
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        //Make another xml element and write subnodes to it
        auto* elementXml = xml->InsertNewChildElement(Name.c_str());
        for (auto& subnode : Subnodes)
        {
            //Stop early if any node fails to write
            bool result = subnode->WriteXml(elementXml, writeNanoforgeMetadata);
            if (!result)
            {
                Log->error("Failed to write xml data for xtbl node \"{}\"", subnode->GetPath());
                return false;
            }
        }

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            elementXml->SetAttribute("__NanoforgeEdited", Edited);
            elementXml->SetAttribute("__NanoforgeNewEntry", NewEntry);
        }

        return true;
    }
};