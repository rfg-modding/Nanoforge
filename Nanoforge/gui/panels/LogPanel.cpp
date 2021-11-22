#include "LogPanel.h"
#include "render/imgui/imgui_ext.h"
#include "util/Profiler.h"

LogPanel::LogPanel()
{

}

LogPanel::~LogPanel()
{

}

#pragma warning(disable:4100)
void LogPanel::Update(GuiState* state, bool* open)
{
    PROFILER_FUNCTION();
    if (!ImGui::Begin("Log", open))
    {
        ImGui::End();
        return;
    }

    //Todo: Come up with a way to automatically enable/disable auto-scroll
    static bool autoScroll = true;
    ImGui::Checkbox("Auto scroll", &autoScroll);

    ImGui::BeginChild("OutputChildWindow", ImVec2(0, 0), true, ImGuiWindowFlags_ChildWindow);

    //Grab the ringbuffer sink off the logger to read recent log messages
    Handle<spdlog::sinks::ringbuffer_sink_mt> sink = std::dynamic_pointer_cast<spdlog::sinks::ringbuffer_sink_mt>(Log->sinks()[2]); //Todo: De-hardcode the ringbuffer sink location
    auto messages = sink->last_formatted();
    for (auto& messageWhole : messages)
    {
        //Hide debug trace messages to avoid filling the log with useless info for users
        if (String::Contains(messageWhole, "[trace]"))
            continue;

        //Parse message contents to color it. Note: Assumes standard spdlog tags ([info], [warn], etc)
        s_view messageShort = s_view(messageWhole).substr(22);
        size_t tagEnd = messageShort.find_first_of(']') + 1;
        s_view messageTag = messageShort.substr(0, tagEnd);
        s_view message = messageShort.substr(tagEnd + 2); //Tags are followed by ": ", so we need to skip that too

        if (messageTag == "[info]")
            ImGui::TextColored(ImVec4(0.0f, 0.7f, 0.0f, 1.0f), string(messageTag));
        else if (messageTag == "[warning]")
            ImGui::TextColored(ImVec4(0.69f, 0.64f, 0.0f, 1.0f), string(messageTag));
        else if (messageTag == "[error]")
            ImGui::TextColored(ImVec4(0.8f, 0.0f, 0.0f, 1.0f), string(messageTag));
        else if (messageTag == "[critical]")
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), string(messageTag));
        else
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), string(messageTag));

        ImGui::SameLine();
        ImGui::Text(string(message)); //Print message. Remove time for log panel to keep things terse. Time is constant sized
    }

    if (autoScroll)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}
#pragma warning(default:4100)