#include "StartPanel.h"
#include "common/filesystem/Path.h"
#include "application/Config.h"
#include "render/imgui/ImGuiFontManager.h"
#include "render/imgui/imgui_ext.h"
#include "application/project/Project.h"
#include "render/imgui/ImGuiConfig.h"
#include "gui/util/WinUtil.h"
#include "util/RfgUtil.h"
#include "gui/util/HelperGuis.h"
#include "gui/misc/Changelog.h"
#include "gui/misc/SettingsGui.h"
#include <imgui_markdown.h>
#include "Log.h"
#include <spdlog/fmt/fmt.h>
#include <imgui/imgui.h>

StartPanel::StartPanel()
{

}

StartPanel::~StartPanel()
{

}

void StartPanel::Update(GuiState* state, bool* open)
{
    if (!ImGui::Begin("Start page", open))
    {
        ImGui::End();
        return;
    }

    if (ImGui::SmallButton("New project"))
        showNewProjectWindow_ = true;
    if (showNewProjectWindow_)
        DrawNewProjectWindow(&showNewProjectWindow_, state->CurrentProject, state->Config);

    ImGui::SameLine();
    if (ImGui::SmallButton("Open project"))
        TryOpenProject(state->CurrentProject, state->Config);

    ImGui::Separator();

    ImGui::Columns(2);
    u32 columnStartY = ImGui::GetCursorPosY();

    //Column 0: Recent projects list + new/open project buttons
    state->FontManager->FontMedium.Push();
    ImGui::Text("Recent projects");
    state->FontManager->FontMedium.Pop();

    DrawRecentProjectsList(state);

    //Column 1: Changelog
    ImGui::NextColumn();
    ImGui::SetCursorPosY(columnStartY);
    state->FontManager->FontMedium.Push();
    ImGui::Text("Changelog");
    state->FontManager->FontMedium.Pop();

    DrawChangelog(state);

    ImGui::End();
}

void StartPanel::DrawDataPathSelector(GuiState* state)
{
    //Warning message shown when data path is invalid
    if (!dataPathValid_)
    {
        state->FontManager->FontMedium.Push();

        ImGui::PushStyleColor(ImGuiCol_Text, gui::Red);
        ImGui::Text(ICON_FA_EXCLAMATION_TRIANGLE " ");
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::TextWrapped("Data folder is invalid. Missing %s. Select a new one with the \"Browse...\" button below.", missingVppName_.c_str());

        state->FontManager->FontMedium.Pop();
    }

    //Data path selector
    string dataPath = state->Config->GetStringReadonly("Data path").value();
    gui::LabelAndValue("Data folder:", dataPath);
    ImGui::SameLine();
    if (ImGui::SmallButton("Browse..."))
    {
        auto output = OpenFolder();
        if (output)
        {
            //Validate new path
            bool result = RfgUtil::ValidateDataPath(output.value(), missingVppName_);
            dataPathValid_ = result;

            //Set data path regardless of validity for now
            auto dataPathVar = state->Config->GetVariable("Data path");
            dataPathVar->Value = output.value();
            state->Config->Save();
        }
    }

    //Draw button that opens the main settings gui with the rest of the settings
    ImGui::SameLine();
    if (ImGui::SmallButton("More settings")) { showSettingsWindow_ = true; }
}

void StartPanel::DrawRecentProjectsList(GuiState* state)
{
    ImGui::Separator();

    //Draw list of recent projects
    auto configVar = state->Config->GetVariable("Recent projects");
    ImVec2 windowSize = ImGui::GetWindowSize();
    if (configVar && ImGui::BeginChild("##RecentProjectsList"))
    {
        auto& recentProjects = std::get<std::vector<string>>(configVar->Value);
        for (auto& path : recentProjects)
        {
            //Draw selectable with no text. Text is manually positioned below this because there doesn't seem to be an easier way to do multi-line selectables
            ImVec2 startPos = ImGui::GetCursorPos();
            if (ImGui::Selectable(fmt::format("##Selectable{}", Path::GetFileName(path)).c_str(), false, 0, {windowSize.x, ImGui::GetFontSize() * 2.4f}))
            {
                if (state->CurrentProject->Load(path))
                {
                    ImGui::EndChild();
                    return;
                }
            }
            ImVec2 endPos = ImGui::GetCursorPos();

            f32 iconIndent = 8.0f;
            ImGui::Indent(iconIndent);
            ImGui::SetCursorPosY(startPos.y + ImGui::GetFontSize() * 0.5f);
            state->FontManager->FontL.Push();
            ImGui::Text(ICON_FA_FILE_INVOICE);
            state->FontManager->FontL.Pop();
            ImGui::Unindent(iconIndent);

            //Draw project name
            f32 textIndent = 34.0f + iconIndent;
            ImGui::Indent(textIndent);
            ImGui::SetCursorPosY(startPos.y);
            state->FontManager->FontMedium.Push();
            ImGui::Text(Path::GetFileName(path));
            state->FontManager->FontMedium.Pop();

            //Draw project path
            ImGui::SetCursorPosY(startPos.y + ImGui::GetFontSize() * 1.45f);
            ImGui::TextColored(gui::SecondaryTextColor, path.c_str());
            ImGui::SetCursorPosY(endPos.y);

            ImGui::Unindent(textIndent);
        }

        ImGui::EndChild();
    }
}