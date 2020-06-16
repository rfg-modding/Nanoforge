#include "MainGui.h"
#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_win32.h>
#include <imgui/examples/imgui_impl_dx11.h>
#include "common/Typedefs.h"
#include "render/imgui/ImGuiFontManager.h"
#include "rfg/PackfileVFS.h"

void MainGui::Update()
{
    //Run gui code
    static bool show_demo_window = true;
    ImGui::ShowDemoWindow(&show_demo_window);


    ImGui::Begin("File explorer");

    fontManager_->FontL.Push();
    ImGui::Text(ICON_FA_ARCHIVE " Packfiles");
    fontManager_->FontL.Pop();
    ImGui::Separator();
    for (auto& packfile : packfileVFS_->packfiles_)
    {
        string packfileNodeLabel = packfile.Name() + " [" + std::to_string(packfile.Header.NumberOfSubfiles) + " subfiles";
        if (packfile.Compressed)
            packfileNodeLabel += ", Compressed";
        if (packfile.Condensed)
            packfileNodeLabel += ", Condensed";

        packfileNodeLabel += "]";

        if (ImGui::TreeNode(packfileNodeLabel.c_str()))
        {
            for (u32 i = 0; i < packfile.Entries.size(); i++)
            {
                ImGui::BulletText("%s - %d bytes", packfile.EntryNames[i], packfile.Entries[i].DataSize);
            }
            ImGui::TreePop();
        }
    }

    ImGui::Separator();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}
