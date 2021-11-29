#pragma once
#include "rfg/xtbl/XtblNodes.h"
#include "Common/Typedefs.h"
#include "XtblDescription.h"
#include "IXtblNode.h"
#include "XtblType.h"
#include "imgui.h"
#include "rfg/xtbl/Xtbl.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"

//Node that can have multiple value types. Similar to a union in C/C++
class ComboElementXtblNode : public IXtblNode
{
public:
#pragma warning(disable:4100) //Disable warning about unused argument. Can't remove the arg since some implementations of this function use it.
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr)
    {
        CalculateEditorValues(xtbl, nameOverride);
        bool editedThisFrame = false; //Used for document unsaved change tracking

        //This type of xtbl node acts like a C/C++ Union, so multiple types can be chosen
        if (ImGui::TreeNode(name_.value().c_str()))
        {
            if (desc_->Description.has_value() && desc_->Description.value() != "")
                gui::TooltipOnPrevious(desc_->Description.value(), nullptr);

            //Get current type. Only one type from the desc is present
            u32 currentType = 0xFFFFFFFF;
            for (u32 i = 0; i < desc_->Subnodes.size(); i++)
                for (auto& subnode : Subnodes)
                    if (String::EqualIgnoreCase(desc_->Subnodes[i]->Name, subnode->Name))
                        currentType = i;

            //Draw a radio button for each type
            for (u32 i = 0; i < desc_->Subnodes.size(); i++)
            {
                bool active = i == currentType;
                if (ImGui::RadioButton(desc_->Subnodes[i]->Name.c_str(), active))
                {
                    Edited = true;
                    editedThisFrame = true;
                    //If different type is selected activate new node and delete current one
                    if (!active)
                    {
                        //Delete current nodes
                        for (auto& subnode : Subnodes)
                            delete subnode;

                        Subnodes.clear();

                        //Create default node
                        auto subdesc = desc_->Subnodes[i];
                        auto subnode = CreateDefaultNode(subdesc, true);
                        subnode->Name = subdesc->Name;
                        subnode->Type = subdesc->Type;
                        subnode->Parent = Parent->GetSubnode(Name);
                        subnode->CategorySet = false;
                        subnode->HasDescription = true;
                        subnode->Enabled = true;
                        Subnodes.push_back(subnode);
                    }
                }
                if (i != desc_->Subnodes.size() - 1)
                    ImGui::SameLine();
            }

            //Draw current type
            for (auto& subnode : Subnodes)
            {
                //Gets parent node name and current node name in path
                string subdescPath = subnode->GetPath();
                subdescPath = subdescPath.substr(String::FindNthCharacterFromBack(subdescPath, '/', 2) + 1);
                Handle<XtblDescription> subdesc = xtbl->GetValueDescription(subdescPath, desc_);
                if (subdesc)
                    if(DrawNodeByDescription(guiState, xtbl, subdesc, this, subdesc->DisplayName.c_str()))
                        editedThisFrame = true;
            }

            ImGui::TreePop();
        }

        return editedThisFrame;
    }
#pragma warning(default:4100)

    virtual void InitDefault()
    {

    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        auto* comboElementXml = xml->InsertNewChildElement(Name.c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            comboElementXml->SetAttribute("__NanoforgeEdited", Edited);
            comboElementXml->SetAttribute("__NanoforgeNewEntry", NewEntry);
        }

        if (Subnodes.size() > 0)
            return Subnodes[0]->WriteXml(comboElementXml, writeNanoforgeMetadata); //Each combo element node should only have 1 subnode
        else
            return true;
    }
};