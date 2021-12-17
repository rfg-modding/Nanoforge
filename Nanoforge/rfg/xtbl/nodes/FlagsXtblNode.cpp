#include "XtblNodes.h"
#include "render/imgui/ImGuiConfig.h"
#include <imgui.h>

#pragma warning(disable:4100) //Disable warning about unused argument. Can't remove the arg since some implementations of this function use it.
bool FlagsXtblNode::DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride)
{
    CalculateEditorValues(xtbl, nameOverride);
    bool editedThisFrame = false; //Used for document unsaved change tracking

    if (ImGui::TreeNode(name_.value().c_str()))
    {
        if (desc_->Description.has_value() && desc_->Description.value() != "")
            gui::TooltipOnPrevious(desc_->Description.value(), nullptr);

        for (auto& subnode : Subnodes)
        {
            auto& flag = std::get<XtblFlag>(subnode->Value);
            if (ImGui::Checkbox(flag.Name.c_str(), &flag.Value))
            {
                Edited = true;
                editedThisFrame = true;
            }
        }

        ImGui::TreePop();
    }

    return editedThisFrame;
}
#pragma warning(default:4100)

void FlagsXtblNode::InitDefault()
{

}

bool FlagsXtblNode::WriteModinfoEdits(tinyxml2::XMLElement* xml)
{
    return WriteXml(xml, false);
}

bool FlagsXtblNode::WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
{
    //Make another xml element and write enabled flags to it
    auto* flagsXml = xml->InsertNewChildElement(Name.c_str());
    for (auto& flag : Subnodes)
    {
        //Skip disabled flags
        XtblFlag flagValue = std::get<XtblFlag>(flag->Value);
        if (!flagValue.Value)
            continue;

        //Write flag to xml
        auto* flagXml = flagsXml->InsertNewChildElement("Flag");
        flagXml->SetText(flagValue.Name.c_str());
    }

    //Store edited state if metadata is enabled
    if (writeNanoforgeMetadata)
    {
        flagsXml->SetAttribute("__NanoforgeEdited", Edited);
        flagsXml->SetAttribute("__NanoforgeNewEntry", NewEntry);
    }

    return true;
}