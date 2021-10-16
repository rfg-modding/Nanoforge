#pragma once
#include "rfg/xtbl/XtblNodes.h"
#include "Common/Typedefs.h"
#include "XtblDescription.h"
#include "IXtblNode.h"
#include "XtblType.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"

//Node found in xtbl description block found at the end of xtbl files
class TableDescriptionXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr)
    {
        return false;
    }

    virtual void InitDefault()
    {

    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        //Write subnodes to modinfo
        for (auto& subnode : Subnodes)
        {
            if (!subnode->Edited)
                continue;

            //Stop early if any node fails to write
            bool result = subnode->WriteModinfoEdits(xml);
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
        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            xml->SetAttribute("__NanoforgeEdited", Edited);
            xml->SetAttribute("__NanoforgeNewEntry", NewEntry);
        }

        //Write subnodes to xml
        for (auto& subnode : Subnodes)
        {
            //Stop early if any node fails to write
            bool result = subnode->WriteXml(xml, writeNanoforgeMetadata);
            if (!result)
            {
                LOG_ERROR("Failed to write xml data for xtbl node \"{}\"", subnode->GetPath());
                return false;
            }
        }
        return true;
    }
};