#include "XtblNodes.h"
#include "rfg/xtbl/XtblDescription.h"
#include <tinyxml2/tinyxml2.h>
#include <imgui.h>

#pragma warning(disable:4100) //Disable warning about unused argument. Can't remove the arg since some implementations of this function use it.
bool IntXtblNode::DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride)
{
    CalculateEditorValues(xtbl, nameOverride);
    bool editedThisFrame = false; //Used for document unsaved change tracking

    if (desc_->Min && desc_->Max)
    {
        if (ImGui::SliderInt(name_.value().c_str(), &std::get<i32>(Value), (i32)desc_->Min.value(), (i32)desc_->Max.value()))
        {
            Edited = true;
            editedThisFrame = true;
        }
    }
    else
    {
        if (ImGui::InputInt(name_.value().c_str(), &std::get<i32>(Value)))
        {
            Edited = true;
            editedThisFrame = true;
        }
    }

    return editedThisFrame;
}
#pragma warning(default:4100)

void IntXtblNode::InitDefault()
{
    Value = 0;
}

bool IntXtblNode::WriteModinfoEdits(tinyxml2::XMLElement* xml)
{
    return WriteXml(xml, false);
}

bool IntXtblNode::WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
{
    auto* intXml = xml->InsertNewChildElement(Name.c_str());
    intXml->SetText(std::to_string(std::get<i32>(Value)).c_str());

    //Store edited state if metadata is enabled
    if (writeNanoforgeMetadata)
    {
        intXml->SetAttribute("__NanoforgeEdited", Edited);
        intXml->SetAttribute("__NanoforgeNewEntry", NewEntry);
    }

    return true;
}