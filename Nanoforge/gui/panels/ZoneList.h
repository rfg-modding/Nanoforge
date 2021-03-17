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
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.134f, 0.160f, 0.196f, 1.0f));
        if (ImGui::BeginChild("##ZoneListFiltersChild", ImVec2(0, 200.0f), true))
        {
            ImGui::Checkbox("Hide empty zones", &hideEmptyZones);
            ImGui::SameLine();
            ImGui::Checkbox("Minimum object count", &hideObjectBelowObjectThreshold);
            if (hideObjectBelowObjectThreshold)
            {
                ImGui::SetNextItemWidth(176.5f);
                ImGui::InputScalar("Min objects to show zone", ImGuiDataType_U32, &minObjectsToShowZone);
            }
            ImGui::EndChild();
        }
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    if (ImGui::Button("Show all"))
    {
        for (auto& zone : state->CurrentTerritory->ZoneFiles)
        {
            zone.RenderBoundingBoxes = true;
        }
        state->CurrentTerritoryUpdateDebugDraw = true;
        state->CurrentTerritory->UpdateObjectClassInstanceCounts();
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide all"))
    {
        for (auto& zone : state->CurrentTerritory->ZoneFiles)
        {
            zone.RenderBoundingBoxes = false;
        }
        state->CurrentTerritoryUpdateDebugDraw = true;
        state->CurrentTerritory->UpdateObjectClassInstanceCounts();
    }

    ImGui::Separator();
    ImGui::InputText("Search", &ZoneList_SearchTerm);

    if (ImGui::BeginTable("Zone list table", 5, ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable
                                              | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable))
    {
        //Todo: Test what this func does
        //ImGui::TableHeader
        ImGui::TableSetupScrollFreeze(0, 1);

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("# Objects", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("District", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_None);
        ImGui::TableHeadersRow();
        
        u32 i = 0;
        for (auto& zone : state->CurrentTerritory->ZoneFiles)
        {
            //Skip zones that don't meet search term or filter requirements
            if (ZoneList_SearchTerm != "" && zone.Name.find(ZoneList_SearchTerm) == string::npos)
                continue;
            if (hideEmptyZones && zone.Zone.Header.NumObjects == 0 || !(hideObjectBelowObjectThreshold ? zone.Zone.Objects.size() >= minObjectsToShowZone : true))
            {
                i++;
                continue;
            }

            ImGui::TableNextRow();

            //Name
            ImGui::TableNextColumn();
            ImGui::Text(zone.ShortName.c_str());

            //# objects
            ImGui::TableNextColumn();
            ImGui::Text("%d", zone.Zone.Header.NumObjects);

            //Controls
            ImGui::TableNextColumn();
            if (ImGui::Checkbox((string("Draw##") + zone.Name).c_str(), &zone.RenderBoundingBoxes))
            {
                state->CurrentTerritoryUpdateDebugDraw = true;
                state->CurrentTerritory->UpdateObjectClassInstanceCounts();
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

            //District
            ImGui::TableNextColumn();
            ImGui::Text("%s", zone.Zone.DistrictName() == "unknown" ? "" : zone.Zone.DistrictNameCstr());

            //Type
            ImGui::TableNextColumn();
            if (zone.MissionLayer)
                ImGui::Text("Mission");
            else if (zone.ActivityLayer)
                ImGui::Text("Activity");
            else
                ImGui::Text("World");

            i++;
        }

        ImGui::EndTable();
    }

    //ImGui::Columns(2);
    //u32 i = 0;
    //for (auto& zone : state->CurrentTerritory->ZoneFiles)
    //{
    //    if (ZoneList_SearchTerm != "" && zone.Name.find(ZoneList_SearchTerm) == string::npos)
    //        continue;

    //    if (hideEmptyZones && zone.Zone.Header.NumObjects == 0 || !(hideObjectBelowObjectThreshold ? zone.Zone.Objects.size() >= minObjectsToShowZone : true))
    //    {
    //        i++;
    //        continue;
    //    }

    //    //Todo: Come up with a way of calculating this value at runtime
    //    const f32 glyphWidth = 9.0f;
    //    ImGui::SetColumnWidth(0, glyphWidth * (f32)state->CurrentTerritory->LongestZoneName);
    //    ImGui::SetColumnWidth(1, 300.0f);
    //    if (ImGui::Selectable(zone.ShortName.c_str()))
    //    {
    //        state->SetSelectedZone(i);
    //        state->SetSelectedZoneObject(nullptr);
    //    }
    //    ImGui::NextColumn();
    //    if (ImGui::Checkbox((string("Draw##") + zone.Name).c_str(), &zone.RenderBoundingBoxes))
    //    {
    //        state->CurrentTerritoryUpdateDebugDraw = true;
    //        state->CurrentTerritory->UpdateObjectClassInstanceCounts();
    //    }
    //    ImGui::SameLine();
    //    if (ImGui::Button((string(ICON_FA_MAP_MARKER "##") + zone.Name).c_str()))
    //    {
    //        if (zone.Zone.Objects.size() > 0)
    //        {
    //            auto& firstObj = zone.Zone.Objects[0];
    //            Vec3 newCamPos = firstObj.Bmin;
    //            newCamPos.x += 50.0f;
    //            newCamPos.y += 100.0f;
    //            newCamPos.z += 50.0f;
    //            state->CurrentTerritoryCamPosNeedsUpdate = true;
    //            state->CurrentTerritoryNewCamPos = newCamPos;
    //        }
    //    }
    //    ImGui::SameLine();
    //    ImGui::Text(" | %d objects | %s", zone.Zone.Header.NumObjects, zone.Zone.DistrictName() == "unknown" ? "" : zone.Zone.DistrictNameCstr());
    //    ImGui::NextColumn();
    //    i++;
    //}
    //ImGui::Columns(1);
    //ImGui::EndChild();

    ImGui::End();
}