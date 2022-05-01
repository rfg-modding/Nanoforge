#include "ZoneList.h"
#include "render/backend/DX11Renderer.h"
#include "common/string/String.h"
#include "common/filesystem/Path.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiFontManager.h"
#include "rfg/Territory.h"
#include "util/Profiler.h"
#include "gui/GuiState.h"
#include "Log.h"

ZoneList::ZoneList()
{
    Title = ICON_FA_MAP " Zones";
}

ZoneList::~ZoneList()
{

}

void ZoneList::Update(GuiState* state, bool* open)
{
    PROFILER_FUNCTION();
    if (!ImGui::Begin(Title.c_str(), open))
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

    static bool hideEmptyZones = true;
    static bool hideObjectBelowObjectThreshold = false;
    static u32 minObjectsToShowZone = 0;
    static bool showMissionAndActivityZones = false;
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
            ImGui::Checkbox("Show missions and activities", &showMissionAndActivityZones);
            ImGui::EndChild();
        }
        ImGui::PopStyleColor();
        ImGui::Separator();
    }

    if (ImGui::Button("Show all"))
    {
        for (ObjectHandle zone : state->CurrentTerritory->Zones)
        {
            zone.Property("RenderBoundingBoxes").Set<bool>(true);
        }
        state->CurrentTerritoryUpdateDebugDraw = true;
        state->CurrentTerritory->UpdateObjectClassInstanceCounts();
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide all"))
    {
        for (ObjectHandle zone : state->CurrentTerritory->Zones)
        {
            zone.Property("RenderBoundingBoxes").Set<bool>(false);
        }
        state->CurrentTerritoryUpdateDebugDraw = true;
        state->CurrentTerritory->UpdateObjectClassInstanceCounts();
    }

    ImGui::Separator();
    ImGui::InputText("Search", &searchTerm_);

    if (ImGui::BeginTable("Zone list table", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable))
    {
        ImGui::TableSetupScrollFreeze(0, 1);

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("# Objects", ImGuiTableColumnFlags_None);
        ImGui::TableSetupColumn("District", ImGuiTableColumnFlags_None);
        ImGui::TableHeadersRow();

        u32 i = 0;
        for (ObjectHandle zone : state->CurrentTerritory->Zones)
        {
            //Skip zones that don't meet search term or filter requirements
            if (searchTerm_ != "" && zone.Property("Name").Get<string>().find(searchTerm_) == string::npos)
                continue;
            if (hideEmptyZones && zone.Property("Objects").GetObjectList().size() == 0 || !(hideObjectBelowObjectThreshold ? zone.Property("Objects").GetObjectList().size() >= minObjectsToShowZone : true))
            {
                i++;
                continue;
            }
            //Hide mission and activity layers if they aren't enabled
            if (!showMissionAndActivityZones && (zone.Property("MissionLayer").Get<bool>() || zone.Property("ActivityLayer").Get<bool>()))
                continue;

            ImGui::TableNextRow();

            //Name
            ImGui::TableNextColumn();
            ImGui::Text(zone.Property("ShortName").Get<string>().c_str());

            //# objects
            ImGui::TableNextColumn();
            ImGui::Text("%d", zone.Property("Objects").GetObjectList().size());

            //District
            ImGui::TableNextColumn();
            ImGui::Text("%s", zone.Property("DistrictName").Get<string>().c_str());

            i++;
        }

        ImGui::EndTable();
    }

    ImGui::End();
}
