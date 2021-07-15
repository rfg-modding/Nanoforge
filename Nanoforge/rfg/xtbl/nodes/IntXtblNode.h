#pragma once
#include "rfg/xtbl/XtblNodes.h"
#include "Common/Typedefs.h"
#include "XtblDescription.h"
#include "IXtblNode.h"
#include "XtblType.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"

//Node with a signed integer value
class IntXtblNode : public IXtblNode
{
public:
    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr)
    {
        CalculateEditorValues(xtbl, nameOverride);

        if (desc_->Min && desc_->Max)
        {
            if (ImGui::SliderInt(name_.value().c_str(), &std::get<i32>(Value), (i32)desc_->Min.value(), (i32)desc_->Max.value()))
                Edited = true;
        }
        else
        {
            if (ImGui::InputInt(name_.value().c_str(), &std::get<i32>(Value)))
                Edited = true;
        }
    }

    virtual void InitDefault()
    {
        Value = 0;
    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
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
};