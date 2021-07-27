#include "HelperGuis.h"
#include "gui/util/WinUtil.h"
#include "render/imgui/ImGuiConfig.h"
#include "render/imgui/imgui_ext.h"
#include "application/project/Project.h"
#include "application/Config.h"
#include "gui/GuiState.h"
#include <filesystem>
#include "imgui.h"
#include "Log.h"

//Set string to defaultValue string if it's empty or only whitespace
void EnsureNotWhitespaceOrEmpty(string& target, string defaultValue)
{
    if (target.empty() || target.find_first_not_of(' ') == string::npos)
        target = defaultValue;
}

bool DrawNewProjectWindow(bool* open, Project* project, Config* config)
{
    if (ImGui::Begin("New project", open))
    {
        static string projectName;
        static string projectPath;
        static string projectDescription;
        static string projectAuthor;
        static bool createProjectFolder = true;
        static bool pathNotSetError = false;

        //Project name/path/etc input
        ImGui::PushItemWidth(230.0f);
        ImGui::InputText("Name:", &projectName);
        ImGui::InputText("Author: ", &projectAuthor);
        ImGui::InputTextMultiline("Description", &projectDescription);
        ImGui::InputText("Path: ", &projectPath);
        ImGui::SameLine();
        if (ImGui::Button("..."))
        {
            auto output = OpenFolder();
            if (output)
                projectPath = output.value();
        }
        ImGui::Checkbox("Create project folder", &createProjectFolder);

        if (pathNotSetError)
        {
            ImGui::TextColored(gui::Red, "Path not valid");
        }

        //Create project from inputs
        if (ImGui::Button("Create"))
        {
            if (projectPath.empty() || projectPath.find_first_not_of(' ') == string::npos || !std::filesystem::exists(projectPath))
            {
                pathNotSetError = true;
            }
            else
            {
                pathNotSetError = false;
                //Set project and save
                string endPath = createProjectFolder ?
                    projectPath + "\\" + projectName + "\\" :
                    projectPath + "\\";
                if (createProjectFolder)
                    std::filesystem::create_directory(endPath);

                EnsureNotWhitespaceOrEmpty(projectName, "ProjectNameNotSet" + std::to_string(time(NULL)));
                EnsureNotWhitespaceOrEmpty(projectDescription, "Not set");
                EnsureNotWhitespaceOrEmpty(projectAuthor, "Not set");

                *project = {};
                project->Name = projectName;
                project->Path = endPath;
                project->Description = projectDescription;
                project->Author = projectAuthor;
                project->ProjectFilename = projectName + ".nanoproj";
                project->UnsavedChanges = false;
                project->Save();
                project->Load(endPath + projectName + ".nanoproj");
                string newPath = endPath + projectName + ".nanoproj";

                //Add project to recent projects list if unique
                config->EnsureVariableExists("Recent projects", ConfigType::List);
                auto& recentProjects = std::get<std::vector<string>>(config->GetVariable("Recent projects")->Value);
                if (std::find(recentProjects.begin(), recentProjects.end(), newPath) == recentProjects.end())
                    recentProjects.push_back(newPath);

                //Save project and return true to signal that it was created
                config->Save();
                *open = false;
                ImGui::End();
                return true;
            }
            //Todo: Add save check for existing project
        }

        ImGui::End();
    }

    return false;
}

void DrawModPackagingPopup(bool* open, GuiState* state)
{
    Project* project = state->CurrentProject;

    if (*open)
        ImGui::OpenPopup("Package mod");
    if (ImGui::BeginPopupModal("Package mod"))
    {
        if (!state->PackfileVFS->Ready())
        {
            ImGui::Text(ICON_FA_EXCLAMATION_CIRCLE " Loading packfiles...");
            ImGui::EndPopup();
            return;
        }

        if (!project->WorkerRunning && !project->WorkerFinished) //Packaging config
        {
            state->FontManager->FontL.Push();
            ImGui::Text(ICON_FA_INFO_CIRCLE " Mod info");
            state->FontManager->FontL.Pop();
            ImGui::Separator();

            ImGui::InputText("Name", &project->Name);
            ImGui::InputText("Author", &project->Author);
            ImGui::InputTextMultiline("Description", &project->Description);
            if (ImGui::Button("Save"))
            {
                project->Save();
            }

            ImGui::Separator();
            state->FontManager->FontL.Push();
            ImGui::Text(ICON_FA_COGS " Options");
            state->FontManager->FontL.Pop();
            ImGui::Separator();

            ImGui::Checkbox("Use MP mod workaround", &project->UseTableWorkaround);
            ImGui::SameLine();
            gui::HelpMarker("This is a temporary way of creating MP mods until there's mod manager support for them. "
                            "If your data folder has table.vpp_pc and you edited any xtbls in there Nanoforge will repack table.vpp_pc with your changes. "
                            "This is necessary since the mod manager overrides table.vpp_pc so edits to xtbls in misc.vpp_pc work. "
                            "Do not check this unless you're trying to mod your MP matches by changing xtbls in table.vpp_pc.",
                            state->FontManager->FontMedium.GetFont());

            if (ImGui::Button("Start packaging"))
            {
                state->SetStatus("Packing mod...", GuiStatus::Working);
                project->PackageMod(project->Path + "\\output\\", state->PackfileVFS, state->Xtbls);
                state->ClearStatus();
            }
            ImGui::SameLine();
        }
        else //Packaging status
        {
            ImGui::Text(project->WorkerState);
            ImGui::ProgressBar(project->WorkerPercentage);
        }

        if (!project->WorkerRunning && project->WorkerFinished)
        {
            if (ImGui::Button("Ok"))
            {
                ImGui::CloseCurrentPopup();
                *open = false;
            }
        }
        else
        {
            if (ImGui::Button("Cancel"))
            {
                *open = false;
                project->PackagingCancelled = true;
                Log->info("Cancelled mod packaging.");
            }
        }

        ImGui::EndPopup();
    }
}

void TryOpenProject(Project* project, Config* config)
{
    auto result = OpenFile("nanoproj");
    if (!result)
        return;

    //Get recent projects config var
    auto recentProjectsVar = config->GetVariable("Recent projects");
    auto& recentProjects = std::get<std::vector<string>>(recentProjectsVar->Value);
    string newPath = result.value();

    //Add project to recent projects list if unique
    bool foundPath = false;
    for (auto& path : recentProjects)
        if (path == newPath)
            foundPath = true;
    if (!foundPath)
        recentProjects.push_back(newPath);

    config->Save();
    project->Load(newPath);
}