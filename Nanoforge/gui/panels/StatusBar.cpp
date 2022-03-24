#include "StatusBar.h"
#include "gui/GuiState.h"
#include "util/TaskScheduler.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiFontManager.h"
#include <stdexcept>

StatusBar::StatusBar()
{

}

StatusBar::~StatusBar()
{

}

void StatusBar::Update(GuiState* state, bool* open)
{
    //Parent window flags
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 viewSize = viewport->WorkSize;
    ImVec2 size = viewport->WorkSize;
    size.y = state->StatusBarHeight;
    ImVec2 pos = viewport->WorkPos;
    pos.y += viewSize.y - state->StatusBarHeight;

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 6.0f));

    //Set color based on status
    switch (state->Status)
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

    ImGui::Begin("Status bar window", open, window_flags);
    ImGui::PopStyleColor();
    ImVec2 startPos = ImGui::GetCursorPos();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    if (ImGui::Button("Tasks"))
        ImGui::OpenPopup("##TaskListPopup");
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    //If custom message is empty, use the default ones
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
    if (state->CustomStatusMessage == "")
    {
        switch (state->Status)
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
        ImGui::Text(state->CustomStatusMessage.c_str());
    }

    //Get task list
    ImVec2 taskListSize = { 300.0f, 300.0f };
    std::vector<Handle<Task>>& threadTasks = TaskScheduler::threadTasks_;
    u32 numTasksRunning = (u32)std::ranges::count_if(threadTasks, [](Handle<Task> task) { return task != nullptr; });

    //Draw task list toggle button
    ImGui::SetNextWindowSizeConstraints({ taskListSize.x, 0.0f }, taskListSize);
    if (ImGui::BeginPopup("##TaskListPopup", ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (numTasksRunning == 0)
        {
            ImGui::TextWrapped("No background tasks are running.");
        }
        else
        {
            u32 numQueuedTasks = (u32)TaskScheduler::taskQueue_.size();
            if (numQueuedTasks > 0)
            {
                ImGui::Text(fmt::format("{} {} tasks queued.", ICON_FA_STOPWATCH, numQueuedTasks).c_str());
                ImGui::Separator();
            }

            for (u32 i = 0; i < threadTasks.size(); i++)
            {
                Handle<Task> task = threadTasks[i];
                if (!task)
                    continue;

                //Task spinner showing that it's in progress
                ImVec2 cursorPos = ImGui::GetCursorPos();
                ImGui::Spinner(fmt::format("##{}", (u64)task.get()).c_str(), 7.0f, 3.0f, gui::SecondaryTextColor);
                ImGui::SameLine();

                //Draw task name to the right of the spinner
                ImGui::SetCursorPosX(cursorPos.x + 20.0f);
                ImGui::Text(task->Name.c_str());
            }
        }

        ImGui::EndPopup();
    }


    ImGui::End();
}