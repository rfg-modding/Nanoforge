#include "HelperGuis.h"
#include "gui/util/WinUtil.h"
#include "render/imgui/ImGuiConfig.h"
#include "render/imgui/imgui_ext.h"
#include "application/project/Project.h"
#include "application/Config.h"
#include "imgui.h"
#include <filesystem>

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
        ImGui::InputText("Path: ", &projectPath);
        ImGui::SameLine();
        if (ImGui::Button("..."))
        {
            auto output = OpenFolder("Pick a folder for your project");
            if (output)
                projectPath = output.value();
        }
        ImGui::InputText("Description", &projectDescription);
        ImGui::InputText("Author: ", &projectAuthor);
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
                if (std::find(recentProjects.begin(), recentProjects.end(), newPath) != recentProjects.end())
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