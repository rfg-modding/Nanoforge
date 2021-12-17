#include "XtblNodes.h"
#include <imgui.h>

#pragma warning(disable:4100) //Disable warning about unused argument. Can't remove the arg since some implementations of this function use it.
bool ColorXtblNode::DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride)
{
    CalculateEditorValues(xtbl, nameOverride);
    bool editedThisFrame = false; //Used for document unsaved change tracking

    if (ImGui::ColorPicker3(name_.value().c_str(), (f32*)&std::get<Vec3>(Value)))
    {
        Edited = true;
        editedThisFrame = true;
    }

    return editedThisFrame;
}
#pragma warning(default:4100)

void ColorXtblNode::InitDefault()
{
    Value = Vec3(0.0f, 0.0f, 0.0f);
}

bool ColorXtblNode::WriteModinfoEdits(tinyxml2::XMLElement* xml)
{
    return WriteXml(xml, false);
}

bool ColorXtblNode::WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
{
    //Make another xml element with <R> <G> <B> sub elements
    auto* colorXml = xml->InsertNewChildElement(Name.c_str());
    auto* r = colorXml->InsertNewChildElement("R");
    auto* g = colorXml->InsertNewChildElement("G");
    auto* b = colorXml->InsertNewChildElement("B");

    //Write values to xml as u8s [0, 255]
    Vec3 vec = std::get<Vec3>(Value);
    r->SetText(std::to_string((u8)(vec.x * 255.0f)).c_str());
    g->SetText(std::to_string((u8)(vec.y * 255.0f)).c_str());
    b->SetText(std::to_string((u8)(vec.z * 255.0f)).c_str());

    //Store edited state if metadata is enabled
    if (writeNanoforgeMetadata)
    {
        colorXml->SetAttribute("__NanoforgeEdited", Edited);
        colorXml->SetAttribute("__NanoforgeNewEntry", NewEntry);
    }

    return true;
}