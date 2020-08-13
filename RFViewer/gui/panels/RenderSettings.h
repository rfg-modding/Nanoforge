#pragma once
#include "gui/GuiState.h"

void RenderSettings_Update(GuiState* state)
{
    if (!ImGui::Begin("Render settings", &state->Visible))
    {
        ImGui::End();
        return;
    }

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_PALETTE " Zone draw settings");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    ImGui::ColorEdit4("Bounding box color", state->BoundingBoxColor);
    ImGui::SliderFloat("Bounding box thickness", &state->BoundingBoxThickness, 0.0f, 16.0f);
    ImGui::ColorEdit3("Label text Color", (f32*)&state->LabelTextColor);
    ImGui::SliderFloat("Label text Size", &state->LabelTextSize, 0.0f, 16.0f);
    ImGui::Checkbox("Draw arrows to parents", &state->DrawParentConnections);

    ImGui::End();
}