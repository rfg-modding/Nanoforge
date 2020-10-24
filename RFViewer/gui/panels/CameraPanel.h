#pragma once
#include "gui/GuiState.h"

void CameraPanel_Update(GuiState* state, bool* open)
{
    if (!ImGui::Begin("Camera", open))
    {
        ImGui::End();
        return;
    }

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_CAMERA " Camera");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    //Todo: Add support for scene system. Add scene selector or delete this and add camera overlay in each scene window
    //f32 fov = state->Camera->GetFov();
    //f32 nearPlane = state->Camera->GetNearPlane();
    //f32 farPlane = state->Camera->GetFarPlane();
    //f32 lookSensitivity = state->Camera->GetLookSensitivity();

    //if (ImGui::Button("0.1")) state->Camera->Speed = 0.1f;
    //ImGui::SameLine();
    //if (ImGui::Button("1.0")) state->Camera->Speed = 1.0f;
    //ImGui::SameLine();
    //if (ImGui::Button("10.0")) state->Camera->Speed = 10.0f;
    //ImGui::SameLine();
    //if (ImGui::Button("25.0")) state->Camera->Speed = 25.0f;
    //ImGui::SameLine();
    //if (ImGui::Button("50.0")) state->Camera->Speed = 50.0f;
    //ImGui::SameLine();
    //if (ImGui::Button("100.0")) state->Camera->Speed = 100.0f;

    //ImGui::InputFloat("Speed", &state->Camera->Speed);
    //ImGui::InputFloat("Sprint speed", &state->Camera->SprintSpeed);
    ////ImGui::InputFloat("Camera smoothing", &camera_->CameraSmoothing);
    //ImGui::Separator();

    //if (ImGui::InputFloat("Fov", &fov))
    //    state->Camera->SetFov(fov);
    //if (ImGui::InputFloat("Near plane", &nearPlane))
    //    state->Camera->SetNearPlane(nearPlane);
    //if (ImGui::InputFloat("Far plane", &farPlane))
    //    state->Camera->SetFarPlane(farPlane);
    //if (ImGui::InputFloat("Look sensitivity", &lookSensitivity))
    //    state->Camera->SetLookSensitivity(lookSensitivity);

    ////ImGui::Checkbox("Follow mouse", &camera_->FollowMouse);
    //ImGui::Separator();

    //if (ImGui::InputFloat3("Position", (float*)&state->Camera->camPosition, 3))
    //{
    //    state->Camera->UpdateViewMatrix();
    //}
    ////gui::LabelAndValue("Position: ", util::to_string(camera_->GetPos()));
    ////gui::LabelAndValue("Target position: ", util::to_string(camera_->GetTargetPos()));
    //gui::LabelAndValue("Aspect ratio: ", std::to_string(state->Camera->GetAspectRatio()));
    //gui::LabelAndValue("Pitch: ", std::to_string(state->Camera->GetPitch()));
    //gui::LabelAndValue("Yaw: ", std::to_string(state->Camera->GetYaw()));

    ImGui::End();
}
