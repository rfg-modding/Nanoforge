#pragma once
#include "gui/GuiState.h"

void RenderSettings_Update(GuiState* state)
{
    if (!ImGui::Begin("Render settings", &state->Visible))
    {
        ImGui::End();
        return;
    }

    state->fontManager_->FontL.Push();
    ImGui::Text(ICON_FA_PALETTE " Zone draw settings");
    state->fontManager_->FontL.Pop();
    ImGui::Separator();

    ImGui::ColorEdit4("Bounding box color", state->boundingBoxColor_);
    ImGui::SliderFloat("Bounding box thickness", &state->boundingBoxThickness_, 0.0f, 16.0f);
    ImGui::ColorEdit3("Label text Color", (f32*)&state->labelTextColor_);
    ImGui::SliderFloat("Label text Size", &state->labelTextSize_, 0.0f, 16.0f);
    ImGui::Checkbox("Draw arrows to parents", &state->drawParentConnections_);

    ImGui::End();
}