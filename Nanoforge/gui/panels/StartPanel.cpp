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
#include "rfg/Tasks.h"
#include "util/Profiler.h"
#include <future>
#include <imgui_markdown.h>
#include "Log.h"
#include <spdlog/fmt/fmt.h>
#include <imgui/imgui.h>
#include <ranges>

StartPanel::StartPanel()
{

}

StartPanel::~StartPanel()
{

}

void StartPanel::Update(GuiState* state, bool* open)
{
    PROFILER_FUNCTION();
    if (!ImGui::Begin("Start page", open))
    {
        ImGui::End();
        return;
    }

    DrawDataPathSelector(state);
    ImGui::Separator();

    if (ImGui::SmallButton("New project"))
    {
        //Close all documents so save confirmation modal appears for them
        for (auto& doc : state->Documents)
            doc->Open = false;

        showNewProjectWindow_ = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Open project"))
    {
        //Close all documents so save confirmation modal appears for them
        for (auto& doc : state->Documents)
            doc->Open = false;

        openProjectRequested_ = true;
    }

    //Show new/open project dialogs once unsaved changes are handled
    u32 numUnsavedDocs = (u32)std::ranges::count_if(state->Documents, [](Handle<IDocument> doc) { return doc->UnsavedChanges; });
    if (showNewProjectWindow_ && numUnsavedDocs == 0)
    {
        bool loadedNewProject = DrawNewProjectWindow(&showNewProjectWindow_, state->CurrentProject, state->Config);
        if (loadedNewProject)
            state->Xtbls->ReloadXtbls();
    }
    if (openProjectRequested_ && numUnsavedDocs == 0)
    {
        bool loadedNewProject = TryOpenProject(state->CurrentProject, state->Config);
        openProjectRequested_ = false;
        if (loadedNewProject)
            state->Xtbls->ReloadXtbls();
    }
    if (openRecentProjectRequested_ && numUnsavedDocs == 0)
    {
        if (std::filesystem::exists(openRecentProjectRequestData_))
        {
            state->CurrentProject->Save();
            state->CurrentProject->Load(openRecentProjectRequestData_);
            state->Xtbls->ReloadXtbls();
        }
        openRecentProjectRequested_ = false;
    }

    ImGui::Separator();
    ImGui::Columns(2);
    u32 columnStartY = (u32)ImGui::GetCursorPosY();

    //Column 0: Recent projects list + new/open project buttons
    state->FontManager->FontL.Push();
    ImGui::Text("Recent projects");
    state->FontManager->FontL.Pop();

    DrawRecentProjectsList(state);

    //Column 1: Changelog
    ImGui::NextColumn();
    ImGui::SetCursorPosY((f32)columnStartY);
    state->FontManager->FontL.Push();
    ImGui::Text("Changelog");
    state->FontManager->FontL.Pop();

    DrawChangelog(state);

    ImGui::End();
}

void StartPanel::DrawDataPathSelector(GuiState* state)
{
    //Check if data path is valid
    auto checkDataPath = [&]()
    {
        auto dataPathVar = state->Config->GetVariable("Data path");
        string dataPath = dataPathVar ? std::get<string>(dataPathVar->Value) : "";
        dataPathValid_ = RfgUtil::ValidateDataPath(dataPath, missingVppName_, false);
    };

    //Startup behavior
    static bool FirstRun = true;
    if (FirstRun)
    {
        //If data path is invalid on startup attempt to auto detect RFG
        checkDataPath();
        if (!state->Config->Exists("Data path") || !dataPathValid_)
            dataPathValid_ = RfgUtil::AutoDetectDataPath(state->Config);

        //Start data folder parse thread
        TaskScheduler::QueueTask(dataFolderParseTask_, std::bind(DataFolderParseTask, dataFolderParseTask_, state));
        FirstRun = false;
    }

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
            dataPathValid_ = RfgUtil::ValidateDataPath(output.value(), missingVppName_);

            //Set data path regardless of validity for now
            auto dataPathVar = state->Config->GetVariable("Data path");
            dataPathVar->Value = output.value();
            state->Config->Save();

            needDataPathParse_ = true;
        }
    }

    //Start data folder parse thread when possible
    if (needDataPathParse_ && !dataFolderParseTask_->Running())
    {
        TaskScheduler::QueueTask(dataFolderParseTask_, std::bind(DataFolderParseTask, dataFolderParseTask_, state));
        needDataPathParse_ = false;
    }
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
                //Close all documents so save confirmation modal appears for them
                for (auto& doc : state->Documents)
                    doc->Open = false;

                openRecentProjectRequested_ = true;
                openRecentProjectRequestData_ = path;
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