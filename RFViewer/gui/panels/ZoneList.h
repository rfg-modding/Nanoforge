#pragma once
#include "gui/GuiState.h"
#include "common/string/String.h"
#include "gui/documents/TerritoryDocument.h"

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

void ZoneList_Update(GuiState* state, bool* open)
{
    if (!ImGui::Begin("Zones", open))
    {
        ImGui::End();
        return;
    }

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_MAP " Zones");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    static int currentTerritory = 0;
    ImGui::Combo("Territory", &currentTerritory, TerritoryList.data(), (int)TerritoryList.size());
    ImGui::SameLine();
    if (ImGui::Button("Open##OpenTerritory"))
    {
        string territoryName = string(TerritoryList[currentTerritory]);
        state->SetTerritory(territoryName);
        state->CreateDocument(Document(territoryName, &TerritoryDocument_Init, &TerritoryDocument_Update, &TerritoryDocument_OnClose, new TerritoryDocumentData
        {
            .TerritoryName = state->CurrentTerritoryName,
            .TerritoryShortname = state->CurrentTerritoryShortname,
            .SceneIndex = (u32)state->Renderer->Scenes.size()
        }));
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

    //Can't draw territory data if no territory is selected/open
    if (!state->CurrentTerritory)
    {
        ImGui::TextWrapped("%s Open a territory above to see its zones.", ICON_FA_EXCLAMATION_CIRCLE);

        ImGui::End();
        return;
    }

    state->FontManager->FontL.Push();
    ImGui::Text(string(ICON_FA_MAP " ") + state->CurrentTerritoryName);
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    ImGui::BeginChild("##Zone file list", ImVec2(0, 0), true);
    if (ImGui::Button("Show all"))
    {
        for (auto& zone : state->CurrentTerritory->ZoneFiles)
        {
            zone.RenderBoundingBoxes = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide all"))
    {
        for (auto& zone : state->CurrentTerritory->ZoneFiles)
        {
            zone.RenderBoundingBoxes = false;
        }
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