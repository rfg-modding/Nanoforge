#include "XtblNodes.h"
#include "rfg/xtbl/XtblDescription.h"
#include "rfg/xtbl/Xtbl.h"
#include <imgui.h>
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"
#include <tinyxml2/tinyxml2.h>
#include "spdlog/fmt/fmt.h"
#include "gui/GuiState.h"

#pragma warning(disable:4100) //Disable warning about unused argument. Can't remove the arg since some implementations of this function use it.
bool StringXtblNode::DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride)
{
    CalculateEditorValues(xtbl, nameOverride);
    bool editedThisFrame = false; //Used for document unsaved change tracking
    if (locale_ != guiState->Localization->CurrentLocale) //Update localized string if locale changes
        CheckForLocalization(guiState);

    //Todo: Add way to change names and auto-update any references. Likely would do this in the XtblDocument stringXml sidebar.
    //Name nodes uneditable for the time being since they're use by xtbl references
    if (desc_->Name == "Name")
    {
        ImGui::InputText(name_.value(), std::get<string>(Value), ImGuiInputTextFlags_ReadOnly);
    }
    else if (nameNoId_.value() == "Description" && (xtbl->Name == "dlc01_tweak_table.xtbl" || xtbl->Name == "online_tweak_table.xtbl" || xtbl->Name == "tweak_table.xtbl"))
    {
        //Todo: Come up with a better way of handling this. Maybe get give all strings big wrapped blocks so they're fully visible
        //Special case for tweak table descriptions
        ImGui::Text("Description:");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, gui::SecondaryTextColor);
        ImGui::TextWrapped(std::get<string>(Value).c_str());
        ImGui::PopStyleColor();
    }
    else
    {
        if (ImGui::InputText(name_.value(), std::get<string>(Value)))
        {
            Edited = true;
            editedThisFrame = true;
        }
    }

    //Display localized string if available
    if (localizedString_.has_value())
    {
        ImGui::Text("Localization:");
        ImGui::SameLine();
        f32 textX = ImGui::GetCursorPosX();

        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + (0.9f * ImGui::GetWindowSize().x));
        ImGui::PushStyleColor(ImGuiCol_Text, gui::SecondaryTextColor);
        ImGui::TextUnformatted(localizedString_.value().c_str());
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();

        ImGui::SetCursorPosX(textX);
        if (ImGui::SmallButton(fmt::format("Copy to clipboard##Localization{}", (u64)this).c_str()))
        {
            ImGui::SetClipboardText(localizedString_.value().c_str());
        }
    }

    return editedThisFrame;
}
#pragma warning(default:4100)

void StringXtblNode::InitDefault()
{
    Value = "";
}

bool StringXtblNode::WriteModinfoEdits(tinyxml2::XMLElement* xml)
{
    return WriteXml(xml, false);
}

bool StringXtblNode::WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
{
    auto* stringXml = xml->InsertNewChildElement(Name.c_str());
    stringXml->SetText(std::get<string>(Value).c_str());

    //Store edited state if metadata is enabled
    if (writeNanoforgeMetadata)
    {
        stringXml->SetAttribute("__NanoforgeEdited", Edited);
        stringXml->SetAttribute("__NanoforgeNewEntry", NewEntry);
    }

    return true;
}

void StringXtblNode::CheckForLocalization(GuiState* guiState)
{
    localizedString_ = guiState->Localization->StringFromKey(std::get<string>(Value));
    locale_ = guiState->Localization->CurrentLocale;
}