#pragma once
#include "gui/GuiState.h"
#include "render/backend/DX11Renderer.h"

void RenderSettings_Update(GuiState* state)
{
    if (!ImGui::Begin("Render settings", &state->Visible))
    {
        ImGui::End();
        return;
    }

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_PALETTE " Zone render settings");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    ImGui::SliderFloat("Bounding box thickness", &state->BoundingBoxThickness, 0.0f, 16.0f);
    ImGui::Checkbox("Draw arrows to parents", &state->DrawParentConnections);

    ImGui::Separator();
    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_MOUNTAIN " Terrain render settings");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    ImGui::Text("Shading mode: ");
    ImGui::SameLine();
    ImGui::RadioButton("Elevation", &state->Renderer->cbPerFrameObject.ShadeMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Diffuse", &state->Renderer->cbPerFrameObject.ShadeMode, 1);

    ImGui::Text("Diffuse presets: ");
    ImGui::SameLine();
    if (ImGui::Button("Default"))
    {
        state->Renderer->cbPerFrameObject.DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        state->Renderer->cbPerFrameObject.DiffuseIntensity = 0.65f;
        state->Renderer->cbPerFrameObject.ElevationFactorBias = 0.8f;
    }
    ImGui::SameLine();
    if (ImGui::Button("False color"))
    {
        state->Renderer->cbPerFrameObject.DiffuseColor = { 1.15f, 0.67f, 0.02f, 1.0f };
        state->Renderer->cbPerFrameObject.DiffuseIntensity = 0.55f;
        state->Renderer->cbPerFrameObject.ElevationFactorBias = -0.8f;
    }

    if (state->Renderer->cbPerFrameObject.ShadeMode != 0)
    {
        ImGui::ColorEdit3("Diffuse", reinterpret_cast<f32*>(&state->Renderer->cbPerFrameObject.DiffuseColor));
        ImGui::InputFloat("Diffuse intensity", &state->Renderer->cbPerFrameObject.DiffuseIntensity);
        ImGui::InputFloat("Elevation color bias", &state->Renderer->cbPerFrameObject.ElevationFactorBias);
    }

    ImGui::End();
}