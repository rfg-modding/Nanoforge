#pragma once
#include "rfg/xtbl/XtblNodes.h"
#include "Common/Typedefs.h"
#include "XtblDescription.h"
#include "IXtblNode.h"
#include "XtblType.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"

//Node with the name of an RFG asset file
class FilenameXtblNode : public IXtblNode
{
public:
#pragma warning(disable:4100) //Disable warning about unused argument. Can't remove the arg since some implementations of this function use it.
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr)
    {
        CalculateEditorValues(xtbl, nameOverride);
        bool editedThisFrame = false; //Used for document unsaved change tracking

        //Todo: Get a list of files with correct format for this node and list those instead of having the player type names out
        if (ImGui::InputText(name_.value(), std::get<string>(Value)))
        {
            Edited = true;
            editedThisFrame = true;
        }

        return editedThisFrame;
    }
#pragma warning(default:4100)

    virtual void InitDefault()
    {
        Value = "";
    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        //Create element with sub-element <Filename>
        auto* nodeXml = xml->InsertNewChildElement(Name.c_str());
        auto* filenameXml = nodeXml->InsertNewChildElement("Filename");
        filenameXml->SetText(std::get<string>(Value).c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            nodeXml->SetAttribute("__NanoforgeEdited", Edited);
            nodeXml->SetAttribute("__NanoforgeNewEntry", NewEntry);
        }

        return true;
    }
};