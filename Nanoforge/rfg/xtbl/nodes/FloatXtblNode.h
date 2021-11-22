#pragma once
#include "rfg/xtbl/XtblNodes.h"
#include "Common/Typedefs.h"
#include "XtblDescription.h"
#include "IXtblNode.h"
#include "XtblType.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"

//Node with a floating point value
class FloatXtblNode : public IXtblNode
{
public:
#pragma warning(disable:4100) //Disable warning about unused argument. Can't remove the arg since some implementations of this function use it.
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr)
    {
        CalculateEditorValues(xtbl, nameOverride);
        bool editedThisFrame = false; //Used for document unsaved change tracking

        if (desc_->Min && desc_->Max)
        {
            if (ImGui::SliderFloat(name_.value().c_str(), &std::get<f32>(Value), desc_->Min.value(), desc_->Max.value()))
            {
                Edited = true;
                editedThisFrame = true;
            }
        }
        else
        {
            if (ImGui::InputFloat(name_.value().c_str(), &std::get<f32>(Value)))
            {
                Edited = true;
                editedThisFrame = true;
            }
        }

        return editedThisFrame;
    }
#pragma warning(default:4100)

    virtual void InitDefault()
    {
        Value = 0.0f;
    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        auto* floatXml = xml->InsertNewChildElement(Name.c_str());
        floatXml->SetText(std::to_string(std::get<f32>(Value)).c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            floatXml->SetAttribute("__NanoforgeEdited", Edited);
            floatXml->SetAttribute("__NanoforgeNewEntry", NewEntry);
        }

        return true;
    }
};