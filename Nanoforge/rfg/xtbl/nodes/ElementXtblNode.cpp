#include "XtblNodes.h"
#include "rfg/xtbl/XtblDescription.h"
#include "render/imgui/ImGuiConfig.h"
#include <tinyxml2/tinyxml2.h>
#include <imgui.h>
#include "Log.h"

bool ElementXtblNode::DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride)
{
    CalculateEditorValues(xtbl, nameOverride);
    bool editedThisFrame = false; //Used for document unsaved change tracking
    auto drawTooltip = [&]()
    {
        if (desc_->Description.has_value() && desc_->Description != "")
            gui::TooltipOnPrevious(desc_->Description.value(), nullptr);
    };
    auto drawNode = [&]()
    {
        for (auto& subdesc : desc_->Subnodes)
            if (DrawNodeByDescription(guiState, xtbl, subdesc, this, nullptr, GetSubnode(subdesc->Name)))
                editedThisFrame = true;
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

    return editedThisFrame;
}

void ElementXtblNode::InitDefault()
{

}

bool ElementXtblNode::WriteModinfoEdits(tinyxml2::XMLElement* xml)
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
            LOG_ERROR("Failed to write modinfo data for xtbl node \"{}\"", subnode->GetPath());
            return false;
        }
    }

    return true;
}

bool ElementXtblNode::WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
{
    //Make another xml element and write subnodes to it
    auto* elementXml = xml->InsertNewChildElement(Name.c_str());
    for (auto& subnode : Subnodes)
    {
        //Stop early if any node fails to write
        bool result = subnode->WriteXml(elementXml, writeNanoforgeMetadata);
        if (!result)
        {
            LOG_ERROR("Failed to write xml data for xtbl node \"{}\"", subnode->GetPath());
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