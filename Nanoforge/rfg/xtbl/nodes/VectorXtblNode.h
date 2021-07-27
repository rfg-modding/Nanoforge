#pragma once
#include "rfg/xtbl/XtblNodes.h"
#include "Common/Typedefs.h"
#include "XtblDescription.h"
#include "IXtblNode.h"
#include "XtblType.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"

//Node with a 3d vector value
class VectorXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr)
    {
        CalculateEditorValues(xtbl, nameOverride);
        bool editedThisFrame = false;

        if (ImGui::InputFloat3(name_.value().c_str(), (f32*)&std::get<Vec3>(Value)))
        {
            Edited = true;
            editedThisFrame = true;
        }

        return editedThisFrame;
    }

    virtual void InitDefault()
    {
        Value = Vec3(0.0f, 0.0f, 0.0f);
    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        //Make another xml element with <X> <Y> <Z> sub elements
        auto* vectorXml = xml->InsertNewChildElement(Name.c_str());
        auto* x = vectorXml->InsertNewChildElement("X");
        auto* y = vectorXml->InsertNewChildElement("Y");
        auto* z = vectorXml->InsertNewChildElement("Z");

        //Write values to xml
        Vec3 vec = std::get<Vec3>(Value);
        x->SetText(std::to_string(vec.x).c_str());
        y->SetText(std::to_string(vec.y).c_str());
        z->SetText(std::to_string(vec.z).c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            vectorXml->SetAttribute("__NanoforgeEdited", Edited);
            vectorXml->SetAttribute("__NanoforgeNewEntry", NewEntry);
        }

        return true;
    }
};