#pragma once
#include "rfg/xtbl/XtblNodes.h"
#include "Common/Typedefs.h"
#include "XtblDescription.h"
#include "IXtblNode.h"
#include "XtblType.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"

//Node with preset options described in xtbl description block
class SelectionXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr)
    {
        CalculateEditorValues(xtbl, nameOverride);
        bool editedThisFrame = false; //Used for document unsaved change tracking

        //Select the first choice if one hasn't been selected
        string& nodeValue = std::get<string>(Value);
        if (nodeValue == "")
            nodeValue = desc_->Choices[0];

        //Draw combo with all possible choices
        if (ImGui::BeginCombo(name_.value().c_str(), std::get<string>(Value).c_str()))
        {
            ImGui::InputText("Search", searchTerm_);
            ImGui::Separator();

            for (auto& choice : desc_->Choices)
            {
                //Check if choice matches seach term
                if (searchTerm_ != "" && !String::Contains(String::ToLower(choice), String::ToLower(searchTerm_)))
                    continue;

                bool selected = choice == nodeValue;
                if (ImGui::Selectable(choice.c_str(), &selected))
                {
                    Value = choice;
                    Edited = true;
                    editedThisFrame = true;
                }
            }
            ImGui::EndCombo();
        }

        return editedThisFrame;
    }

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
        auto* selectionXml = xml->InsertNewChildElement(Name.c_str());
        selectionXml->SetText(std::get<string>(Value).c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            selectionXml->SetAttribute("__NanoforgeEdited", Edited);
            selectionXml->SetAttribute("__NanoforgeNewEntry", NewEntry);
        }

        return true;
    }

private:
    string searchTerm_ = "";
};