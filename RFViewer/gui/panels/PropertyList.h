#pragma once
#include "gui/GuiState.h"
#include "render/imgui/ImGuiConfig.h"
#include "RfgTools++/formats/zones/properties/primitive/StringProperty.h"
#include "RfgTools++/formats/zones/properties/primitive/BoolProperty.h"
#include "RfgTools++/formats/zones/properties/primitive/FloatProperty.h"
#include "RfgTools++/formats/zones/properties/primitive/UintProperty.h"
#include "RfgTools++/formats/zones/properties/compound/Vec3Property.h"
#include "RfgTools++/formats/zones/properties/compound/Matrix33Property.h"
#include "RfgTools++/formats/zones/properties/compound/BoundingBoxProperty.h"
#include "RfgTools++/formats/zones/properties/compound/OpProperty.h"
#include "RfgTools++/formats/zones/properties/special/DistrictFlagsProperty.h"
#include "RfgTools++/formats/zones/properties/compound/ListProperty.h"
#include "RfgTools++/formats/zones/properties/special/NavpointDataProperty.h"

void PropertyList_Update(GuiState* state, bool* open)
{
    if (!ImGui::Begin("Properties", open))
    {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_WRENCH " Properties");
    state->FontManager->FontL.Pop();

    if (state->PropertyPanelContentFuncPtr)
    {
        state->PropertyPanelContentFuncPtr(state);
    }
    else
    {
        ImGui::Text("%s Click a file in the file explorer or another panel to see it's properties here.", ICON_FA_EXCLAMATION_CIRCLE);
    }

    ImGui::End();
}