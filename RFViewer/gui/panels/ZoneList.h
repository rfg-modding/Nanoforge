#pragma once
#include "gui/GuiState.h"

void ZoneList_Update(GuiState* state)
{
    if (!ImGui::Begin("Zones", &state->Visible))
    {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_MAP " Zones");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    static bool hideEmptyZones = true;
    ImGui::Checkbox("Hide empty zones", &hideEmptyZones);
    ImGui::SameLine();
    static bool hideObjectBelowObjectThreshold = true;
    ImGui::Checkbox("Minimum object count", &hideObjectBelowObjectThreshold);
    static u32 minObjectsToShowZone = 2;
    if (hideObjectBelowObjectThreshold)
    {
        ImGui::SetNextItemWidth(176.5f);
        ImGui::InputScalar("Min objects to show zone", ImGuiDataType_U32, &minObjectsToShowZone);
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.134f, 0.160f, 0.196f, 1.0f));
    ImGui::BeginChild("##Zone file list", ImVec2(0, 0), true);
    if (ImGui::Button("Show all"))
    {
        for (auto& zone : state->ZoneManager->ZoneFiles)
        {
            zone.RenderBoundingBoxes = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide all"))
    {
        for (auto& zone : state->ZoneManager->ZoneFiles)
        {
            zone.RenderBoundingBoxes = false;
        }
    }

    ImGui::Columns(2);
    u32 i = 0;
    for (auto& zone : state->ZoneManager->ZoneFiles)
    {
        if (hideEmptyZones && zone.Zone.Header.NumObjects == 0 || !(hideObjectBelowObjectThreshold ? zone.Zone.Objects.size() >= minObjectsToShowZone : true))
        {
            i++;
            continue;
        }

        ImGui::SetColumnWidth(0, 200.0f);
        ImGui::SetColumnWidth(1, 300.0f);
        if (ImGui::Selectable(zone.Name.c_str()))
        {
            state->SetSelectedZone(i);
        }
        ImGui::NextColumn();
        ImGui::Checkbox((string("Draw##") + zone.Name).c_str(), &zone.RenderBoundingBoxes);
        ImGui::SameLine();
        if (ImGui::Button((string(ICON_FA_MAP_MARKER "##") + zone.Name).c_str()))
        {
            if (zone.Zone.Objects.size() > 0)
            {
                auto& firstObj = zone.Zone.Objects[0];
                Vec3 newCamPos = firstObj.Bmin;
                newCamPos.x += 50.0f;
                newCamPos.y += 100.0f;
                newCamPos.z += 50.0f;
                state->Camera->SetPosition(newCamPos.x, newCamPos.y, newCamPos.z);
            }
        }
        ImGui::SameLine();
        ImGui::Text(" | %d objects | %s", zone.Zone.Header.NumObjects, zone.Zone.DistrictName() == "unknown" ? "" : zone.Zone.DistrictNameCstr());
        ImGui::NextColumn();
        i++;
    }
    ImGui::Columns(1);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::End();
}