#include "XtblNodes.h"
#include "render/imgui/ImGuiConfig.h"
#include "rfg/xtbl/XtblDescription.h"
#include "common/string/String.h"
#include <tinyxml2/tinyxml2.h>
#include "rfg/xtbl/Xtbl.h"
#include <imgui.h>
#include "Log.h"

#pragma warning(disable:4100) //Disable warning about unused argument. Can't remove the arg since some implementations of this function use it.
bool ListXtblNode::DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride)
{
    CalculateEditorValues(xtbl, nameOverride);
    bool editedThisFrame = false; //Used for document unsaved change tracking

    //Draw subnodes
    string name = nameNoId_.value() + fmt::format(" [List]##{}", (u64)this);
    if (ImGui::TreeNode(name.c_str()))
    {
        if (desc_->Description.has_value() && desc_->Description.value() != "")
            gui::TooltipOnPrevious(desc_->Description.value(), nullptr);

        //Get description of subnodes
        string subdescPath = desc_->Subnodes[0]->GetPath();
        subdescPath = subdescPath.substr(String::FindNthCharacterFromBack(subdescPath, '/', 2) + 1);
        Handle<XtblDescription> subdesc = xtbl->GetValueDescription(subdescPath, desc_);

        //Add another item to the list
        if (ImGui::Button("Add item"))
        {
            IXtblNode* newSubnode = CreateDefaultNode(subdesc, true);
            newSubnode->Parent = this;
            Subnodes.push_back(newSubnode);
        }

        //Draw each item on the list
        u32 deletedItemIndex = 0xFFFFFFFF;
        for (u32 i = 0; i < Subnodes.size(); i++)
        {
            auto& subnode = Subnodes[i];
            string subnodeName = subnode->Name;
            auto nameValue = subnode->GetSubnodeValueString("Name");
            if (nameValue)
                subnodeName = nameValue.value();

            //Right click menu used for each list item
            auto drawRightClickMenu = [&](const char* rightClickMenuId)
            {
                //Draw right click context menu
                if (ImGui::BeginPopupContextItem(rightClickMenuId))
                {
                    if (ImGui::MenuItem("Delete"))
                        deletedItemIndex = i;

                    ImGui::EndPopup();
                }
            };

            //Draw list items
            if (subdesc)
            {
                string rightClickMenuId = fmt::format("##RightClickMenu{}", (u64)subnode);
                //Use tree nodes for element and list subnodes since they consist of many values
                if (subdesc->Type == XtblType::Element || subdesc->Type == XtblType::List)
                {
                    if (ImGui::TreeNode(fmt::format("{}##{}", subnodeName, (u64)subnode).c_str()))
                    {
                        drawRightClickMenu(rightClickMenuId.c_str());
                        if (DrawNodeByDescription(guiState, xtbl, subdesc, this, subnodeName.c_str(), subnode))
                            editedThisFrame = true;

                        ImGui::TreePop();
                    }
                    else
                    {
                        drawRightClickMenu(rightClickMenuId.c_str());
                    }

                }
                else //Don't use tree nodes for primitives since they're a single value
                {
                    if (DrawNodeByDescription(guiState, xtbl, subdesc, this, subnodeName.c_str(), subnode))
                        editedThisFrame = true;

                    drawRightClickMenu(rightClickMenuId.c_str());
                }
            }
        }

        //Handle item deletion
        if (deletedItemIndex != 0xFFFFFFFF)
        {
            //Get deleted node, delete it and it's subnodes, and mark the list as edited
            IXtblNode* deletedNode = Subnodes[deletedItemIndex];
            Subnodes.erase(Subnodes.begin() + deletedItemIndex);
            delete deletedNode;
            this->Edited = true;
            editedThisFrame = true;
        }

        ImGui::TreePop();
    }

    return editedThisFrame;
}
#pragma warning(default: 4100)

void ListXtblNode::InitDefault()
{

}

void ListXtblNode::MarkAsEditedRecursive(IXtblNode* node)
{
    node->Edited = true;
    for (auto& subnode : node->Subnodes)
        MarkAsEditedRecursive(subnode);
}

bool ListXtblNode::WriteModinfoEdits(tinyxml2::XMLElement* xml)
{
    //Note: Currently the whole list is replaced if one item is changed since not every list item has unique identifiers
    //Make another xml element and write all subnodes to it
    auto* listXml = xml->InsertNewChildElement(Name.c_str());
    listXml->SetAttribute("LIST_ACTION", "REPLACE");
    for (auto& subnode : Subnodes)
    {
        //Mark subnodes as edited to ensure all their subnodes are written.
        //Required since the current method of editing lists is just replacing them entirely.
        MarkAsEditedRecursive(subnode);

        //Stop early if any node fails to write
        bool result = subnode->WriteModinfoEdits(listXml);
        if (!result)
        {
            LOG_ERROR("Failed to write modinfo data for xtbl node \"{}\"", subnode->GetPath());
            return false;
        }
    }

    return true;
}

bool ListXtblNode::WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
{
    //Make another xml element and write edited subnodes to it
    auto* listXml = xml->InsertNewChildElement(Name.c_str());
    for (auto& subnode : Subnodes)
    {
        //Stop early if any node fails to write
        bool result = subnode->WriteXml(listXml, writeNanoforgeMetadata);
        if (!result)
        {
            LOG_ERROR("Failed to write modinfo data for xtbl node \"{}\"", subnode->GetPath());
            return false;
        }
    }

    //Store edited state if metadata is enabled
    if (writeNanoforgeMetadata)
    {
        listXml->SetAttribute("__NanoforgeEdited", Edited);
        listXml->SetAttribute("__NanoforgeNewEntry", NewEntry);
    }

    return true;
}