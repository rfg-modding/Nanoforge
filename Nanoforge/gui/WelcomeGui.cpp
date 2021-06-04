#include "WelcomeGui.h"
#include "common/filesystem/Path.h"
#include "application/Config.h"
#include "render/imgui/ImGuiFontManager.h"
#include "render/imgui/imgui_ext.h"
#include "application/project/Project.h"
#include "render/imgui/ImGuiConfig.h"
#include "gui/util/WinUtil.h"
#include "util/RfgUtil.h"
#include "gui/util/HelperGuis.h"
#include "Log.h"
#include <spdlog/fmt/fmt.h>
#include <imgui/imgui.h>

void WelcomeGui::Init(Config* config, ImGuiFontManager* fontManager, Project* project, u32 windowWidth, u32 windowHeight)
{
    config_ = config;
    fontManager_ = fontManager;
    project_ = project;
    windowWidth_ = windowWidth;
    windowHeight_ = windowHeight;
    config_->EnsureVariableExists("Recent projects", ConfigType::String);

    //Check if data path is valid
    auto dataPathVar = config_->GetVariable("Data path");
    string dataPath = dataPathVar ? std::get<string>(dataPathVar->Value) : "";
    dataPathValid_ = RfgUtil::ValidateDataPath(dataPath, missingVppName_, false);

    //If data path is invalid attempt to locate it from a list of common install locations
    if (!config_->Exists("Data path") || !dataPathValid_)
        dataPathValid_ = RfgUtil::AutoDetectDataPath(config_);
}

void WelcomeGui::Update()
{
    //Draw new project window
    if (showNewProjectWindow_)
        DrawNewProjectWindow(&showNewProjectWindow_, project_, config_);

    //Create imgui window that fills the whole app window
    ImGui::SetNextWindowPos({ 0, 0 });
    ImGui::SetNextWindowSize(ImVec2(windowWidth_, windowHeight_));
    if (!ImGui::Begin("Welcome", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    //Draw welcome gui where the user can create a new project or load recent ones
    ImGui::Text("Welcome to Nanoforge");
    ImGui::Separator();

    ImGui::BeginChild("##WelcomeColumn0", ImVec2(windowWidth_, windowHeight_ * 0.92f), true, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Open or create a project to continue");
    if (ImGui::Button("New project"))
    {
        showNewProjectWindow_ = true;
    }

    DrawDataPathSelector();
    DrawRecentProjectsList();
    
    ImGui::EndChild();
    ImGui::End();
}

void WelcomeGui::DrawDataPathSelector()
{
    ImGui::Separator();

    //Warning message shown when data path is invalid
    if (!dataPathValid_)
    {
        fontManager_->FontMedium.Push();

        ImGui::PushStyleColor(ImGuiCol_Text, gui::Red);
        ImGui::Text(ICON_FA_EXCLAMATION_TRIANGLE " ");
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::TextWrapped("Data path is invalid. Missing %s. Select a new one with the \"Browse...\" button below.", missingVppName_.c_str());
        
        fontManager_->FontMedium.Pop();
    }

    //Data path selector
    string dataPath = config_->GetStringReadonly("Data path").value();
    gui::LabelAndValue("Data folder", dataPath);
    ImGui::SameLine();
    if (ImGui::Button("Browse..."))
    {
        auto output = OpenFolder("Select your RFGR data folder");
        if (output)
        {
            //Validate new path
            bool result = RfgUtil::ValidateDataPath(output.value(), missingVppName_);
            dataPathValid_ = result;

            //Set data path regardless of validity for now
            auto dataPathVar = config_->GetVariable("Data path");
            dataPathVar->Value = output.value();
            config_->Save();
        }
    }
}

void WelcomeGui::DrawRecentProjectsList()
{
    ImGui::Separator();
    fontManager_->FontL.Push();
    ImGui::Text(ICON_FA_FILE_IMPORT "Recent projects");
    fontManager_->FontL.Pop();

    //Draw list of recent projects
    auto configVar = config_->GetVariable("Recent projects");
    if (configVar && ImGui::BeginChild("##RecentProjectsList", { windowWidth_ * 0.95f, windowHeight_ * 0.6f }, true))
    {
        auto& recentProjects = std::get<std::vector<string>>(configVar->Value);
        for (auto& path : recentProjects)
        {
            if (ImGui::Selectable(fmt::format("{} - \"{}\"", Path::GetFileName(path), path).c_str(), false))
            {
                if (project_->Load(path))
                {
                    Done = true; //Signal to main loop to move to next stage of the app
                    ImGui::EndChild();
                    ImGui::End();
                    return;
                }
            }
        }

        ImGui::EndChild();
    }
}

void WelcomeGui::HandleResize(u32 width, u32 height)
{
    windowWidth_ = width;
    windowHeight_ = height;
}