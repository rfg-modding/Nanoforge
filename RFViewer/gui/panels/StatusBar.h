#pragma once
#include "gui/GuiState.h"

void StatusBar_Update(GuiState* state)
{
    //Parent window flags
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 viewSize = viewport->GetWorkSize();
    ImVec2 size = viewport->GetWorkSize();
    size.y = state->statusBarHeight_;
    ImVec2 pos = viewport->GetWorkPos();
    pos.y += viewSize.y - state->statusBarHeight_;

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(viewport->ID);


    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 6.0f));

    //Set color based on status
    switch (state->status_)
    {
    case Ready:
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.48f, 0.8f, 1.0f));
        break;
    case Working:
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.79f, 0.32f, 0.0f, 1.0f));
        break;
    case Error:
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
        break;
    default:
        throw std::out_of_range("Error! Status enum in MainGui has an invalid value!");
    }

    ImGui::Begin("Status bar window", &state->Visible, window_flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    //If custom message is empty, use the default ones
    if (state->customStatusMessage_ == "")
    {
        switch (state->status_)
        {
        case Ready:
            ImGui::Text(ICON_FA_CHECK " Ready");
            break;
        case Working:
            ImGui::Text(ICON_FA_SYNC " Working");
            break;
        case Error:
            ImGui::Text(ICON_FA_EXCLAMATION_TRIANGLE " Error");
            break;
        default:
            throw std::out_of_range("Error! Status enum in MainGui has an invalid value!");
        }
    }
    else //Else use custom one
    {
        ImGui::Text(state->customStatusMessage_.c_str());
    }

    ImGui::End();
}