#pragma once
#include "gui/GuiState.h"
#include "render/backend/DX11Renderer.h"
#include "common/string/String.h"

static string ZoneList_SearchTerm = "";

void ZoneList_Update(GuiState* state, bool* open)
{
    if (!ImGui::Begin("Zones", open))
    {
        ImGui::End();
        return;
    }

    //Can't draw territory data if no territory is selected/open
    if (!state->CurrentTerritory)
    {
        ImGui::TextWrapped("%s Open a territory view \"Tools > Open territory\" to see its zones.", ICON_FA_EXCLAMATION_CIRCLE);

        ImGui::End();
        return;
    }

    state->FontManager->FontL.Push();
    ImGui::Text(string(ICON_FA_MAP " ") + Path::GetFileNameNoExtension(state->CurrentTerritoryShortname));
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    static bool hideEmptyZones = true;
    static bool hideObjectBelowObjectThreshold = true;
    static u32 minObjectsToShowZone = 2;
    if (ImGui::CollapsingHeader("Filters"))
    {
        ImGui::Checkbox("Hide empty zones", &hideEmptyZones);
        ImGui::SameLine();
        ImGui::Checkbox("Minimum object count", &hideObjectBelowObjectThreshold);
        if (hideObjectBelowObjectThreshold)
        {
            ImGui::SetNextItemWidth(176.5f);
            ImGui::InputScalar("Min objects to show zone", ImGuiDataType_U32, &minObjectsToShowZone);
        }
        ImGui::Separator();
    }

    ImGui::BeginChild("##Zone file list", ImVec2(0, 0), true);
    if (ImGui::Button("Show all"))
    {
        for (auto& zone : state->CurrentTerritory->ZoneFiles)
        {
            zone.RenderBoundingBoxes = true;
        }
        state->CurrentTerritoryUpdateDebugDraw = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide all"))
    {
        for (auto& zone : state->CurrentTerritory->ZoneFiles)
        {
            zone.RenderBoundingBoxes = false;
        }
        state->CurrentTerritoryUpdateDebugDraw = true;
    }

    ImGui::Separator();
    ImGui::InputText("Search", &ZoneList_SearchTerm);

    ImGui::Columns(2);
    u32 i = 0;
    for (auto& zone : state->CurrentTerritory->ZoneFiles)
    {
        if (ZoneList_SearchTerm != "" && zone.Name.find(ZoneList_SearchTerm) == string::npos)
            continue;

        if (hideEmptyZones && zone.Zone.Header.NumObjects == 0 || !(hideObjectBelowObjectThreshold ? zone.Zone.Objects.size() >= minObjectsToShowZone : true))
        {
            i++;
            continue;
        }

        //Todo: Come up with a way of calculating this value at runtime
        const f32 glyphWidth = 9.0f;
        ImGui::SetColumnWidth(0, glyphWidth * (f32)state->CurrentTerritory->LongestZoneName);
        ImGui::SetColumnWidth(1, 300.0f);
        if (ImGui::Selectable(zone.ShortName.c_str()))
        {
            state->SetSelectedZone(i);
            state->SetSelectedZoneObject(nullptr);
        }
        ImGui::NextColumn();
        if (ImGui::Checkbox((string("Draw##") + zone.Name).c_str(), &zone.RenderBoundingBoxes))
        {
            state->CurrentTerritoryUpdateDebugDraw = true;
        }
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
                state->CurrentTerritoryCamPosNeedsUpdate = true;
                state->CurrentTerritoryNewCamPos = newCamPos;
            }
        }
        ImGui::SameLine();
        ImGui::Text(" | %d objects | %s", zone.Zone.Header.NumObjects, zone.Zone.DistrictName() == "unknown" ? "" : zone.Zone.DistrictNameCstr());
        ImGui::NextColumn();
        i++;
    }
    ImGui::Columns(1);
    ImGui::EndChild();

    ImGui::End();
}