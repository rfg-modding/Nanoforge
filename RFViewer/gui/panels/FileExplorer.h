#pragma once
#include "gui/GuiState.h"

void FileExplorer_Update(GuiState* state, bool* open)
{
    if (!ImGui::Begin("File explorer", open))
    {
        ImGui::End();
        return;
    }

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_ARCHIVE " Packfiles");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    for (auto& packfile : state->PackfileVFS->packfiles_)
    {
        string packfileNodeLabel = packfile.Name() + " [" + std::to_string(packfile.Header.NumberOfSubfiles) + " subfiles";
        if (packfile.Compressed)
            packfileNodeLabel += ", Compressed";
        if (packfile.Condensed)
            packfileNodeLabel += ", Condensed";

        packfileNodeLabel += "]";

        if (ImGui::TreeNodeEx(packfileNodeLabel.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
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