#pragma once
#include "gui/GuiState.h"

void CameraPanel_Update(GuiState* state)
{
    if (!ImGui::Begin("Camera", &state->Visible))
    {
        ImGui::End();
        return;
    }

    state->fontManager_->FontL.Push();
    ImGui::Text(ICON_FA_CAMERA " Camera");
    state->fontManager_->FontL.Pop();
    ImGui::Separator();

    f32 fov = state->camera_->GetFov();
    f32 nearPlane = state->camera_->GetNearPlane();
    f32 farPlane = state->camera_->GetFarPlane();
    f32 lookSensitivity = state->camera_->GetLookSensitivity();

    if (ImGui::Button("0.1")) state->camera_->Speed = 0.1f;
    ImGui::SameLine();
    if (ImGui::Button("1.0")) state->camera_->Speed = 1.0f;
    ImGui::SameLine();
    if (ImGui::Button("10.0")) state->camera_->Speed = 10.0f;
    ImGui::SameLine();
    if (ImGui::Button("25.0")) state->camera_->Speed = 25.0f;
    ImGui::SameLine();
    if (ImGui::Button("50.0")) state->camera_->Speed = 50.0f;
    ImGui::SameLine();
    if (ImGui::Button("100.0")) state->camera_->Speed = 100.0f;

    ImGui::InputFloat("Speed", &state->camera_->Speed);
    ImGui::InputFloat("Sprint speed", &state->camera_->SprintSpeed);
    //ImGui::InputFloat("Camera smoothing", &camera_->CameraSmoothing);
    ImGui::Separator();

    if (ImGui::InputFloat("Fov", &fov))
        state->camera_->SetFov(fov);
    if (ImGui::InputFloat("Near plane", &nearPlane))
        state->camera_->SetNearPlane(nearPlane);
    if (ImGui::InputFloat("Far plane", &farPlane))
        state->camera_->SetFarPlane(farPlane);
    if (ImGui::InputFloat("Look sensitivity", &lookSensitivity))
        state->camera_->SetLookSensitivity(lookSensitivity);

    //ImGui::Checkbox("Follow mouse", &camera_->FollowMouse);
    ImGui::Separator();

    if (ImGui::InputFloat3("Position", (float*)&state->camera_->camPosition, 3))
    {
        state->camera_->UpdateViewMatrix();
    }
    //gui::LabelAndValue("Position: ", util::to_string(camera_->GetPos()));
    //gui::LabelAndValue("Target position: ", util::to_string(camera_->GetTargetPos()));
    gui::LabelAndValue("Aspect ratio: ", std::to_string(state->camera_->GetAspectRatio()));
    gui::LabelAndValue("Pitch: ", std::to_string(state->camera_->GetPitch()));
    gui::LabelAndValue("Yaw: ", std::to_string(state->camera_->GetYaw()));

    ImGui::End();
}
