#include "LocalizationDocument.h"
#include "render/imgui/imgui_ext.h"
#include "BinaryTools/BinaryReader.h"
#include "gui/GuiState.h"
#include "RfgTools++/hashes/Hash.h"
#include "Log.h"

LocalizationDocument::LocalizationDocument(GuiState* state)
{
    selectedLocale_ = state->Localization->CurrentLocale;
}

LocalizationDocument::~LocalizationDocument()
{

}

void LocalizationDocument::Update(GuiState* state)
{
    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_LANGUAGE " Strings");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    //Search bar + options
    ImGui::Checkbox("Search by identifier", &searchByHash_);
    if (ImGui::InputText("Search", &searchTerm_))
        searchHash_ = Hash::HashVolitionCRCAlt(searchTerm_, 0);

    //Locale selector
    if (ImGui::BeginCombo("Locale", state->Localization->GetLocaleName(selectedLocale_).c_str()))
    {
        for (auto& localeClass : state->Localization->Classes)
        {
            bool selected = localeClass.Type == selectedLocale_;
            if (ImGui::Selectable(localeClass.Name.c_str(), &selected))
                selectedLocale_ = localeClass.Type;
        }

        ImGui::EndCombo();
    }

    ImGui::Separator();

    if (!state->Localization->Ready())
    {
        ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Localization data not yet loaded.");
        ImGui::End();
        return;
    }

    //Get selected locale
    LocalizationClass* localeClass = state->Localization->GetLocale(selectedLocale_);
    if (!localeClass)
    {
        ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Failed to get locale data!");
        ImGui::End();
        return;
    }

    //Draw locale strings
    u32 i = 0;
    for (auto& str : localeClass->Strings)
    {
        //Check if entry matches search
        if (searchTerm_ != "")
        {
            if (searchByHash_ && searchHash_ != str.KeyHash)
                continue;
            else if (!searchByHash_ && !String::Contains(String::ToLower(str.String), String::ToLower(searchTerm_)))
                continue;
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + (0.9f * ImGui::GetWindowSize().x));
        ImGui::TextUnformatted(str.String.c_str());
        ImGui::PopTextWrapPos();
        if (ImGui::SmallButton(fmt::format("Copy to clipboard##{}", i).c_str()))
        {
            ImGui::SetClipboardText(str.String.c_str());
        }
        ImGui::Separator();
        i++;
    }
}

void LocalizationDocument::Save(GuiState* state)
{

}