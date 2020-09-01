#pragma once
#include "gui/GuiState.h"
#include "common/string/String.h"

static string ZoneList_SearchTerm = "";
static std::vector<const char*> TerritoryList =
{
    "terr01",
    "dlc01",
    "mp_cornered",
    "mp_crashsite",
    "mp_crescent",
    "mp_crevice",
    "mp_deadzone",
    "mp_downfall",
    "mp_excavation",
    "mp_fallfactor",
    "mp_framework",
    "mp_garrison",
    "mp_gauntlet",
    "mp_overpass",
    "mp_pcx_assembly",
    "mp_pcx_crossover",
    "mp_pinnacle",
    "mp_quarantine",
    "mp_radial",
    "mp_rift",
    "mp_sandpit",
    "mp_settlement",
    "mp_warlords",
    "mp_wasteland",
    "mp_wreckage",
    "mpdlc_broadside",
    "mpdlc_division",
    "mpdlc_islands",
    "mpdlc_landbridge",
    "mpdlc_minibase",
    "mpdlc_overhang",
    "mpdlc_puncture",
    "mpdlc_ruins",
    "wc1",
    "wc2",
    "wc3",
    "wc4",
    "wc5",
    "wc6",
    "wc7",
    "wc8",
    "wc9",
    "wc10",
    "wcdlc1",
    "wcdlc2",
    "wcdlc3",
    "wcdlc4",
    "wcdlc5",
    "wcdlc6",
    "wcdlc7",
    "wcdlc8",
    "wcdlc9"
};

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

    static int currentTerritory = 0;
    ImGui::Combo("Territory", &currentTerritory, TerritoryList.data(), TerritoryList.size());
    ImGui::SameLine();
    if (ImGui::Button("Set##SetTerritory"))
    {
        state->SetTerritory(string(TerritoryList[currentTerritory]));
    }
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

    ImGui::Separator();
    ImGui::InputText("Search", &ZoneList_SearchTerm);

    ImGui::Columns(2);
    u32 i = 0;
    for (auto& zone : state->ZoneManager->ZoneFiles)
    {
        if (ZoneList_SearchTerm != "" && zone.Name.find(ZoneList_SearchTerm) == string::npos)
            continue;

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
            state->SetSelectedZoneObject(nullptr);
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