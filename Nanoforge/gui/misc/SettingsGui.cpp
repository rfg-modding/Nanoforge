#include "SettingsGui.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "common/filesystem/Path.h"
#include "render/imgui/ImGuiConfig.h"
#include "application/Config.h"
#include "render/imgui/ImGuiFontManager.h"
#include "gui/util/WinUtil.h"
#include <spdlog/fmt/fmt.h>

void DrawSettingsGui(bool* open, ImGuiFontManager* fonts)
{
    ImGui::SetNextWindowFocus();
    if (!*open)
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !ImGui::IsAnyItemActive())
    {
        *open = false; //Close with escape if no item is being edited
        return;
    }

    ImGui::OpenPopup("Settings");
    ImVec2 viewportSize = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowPos({ viewportSize.x / 2.7f, viewportSize.y / 2.7f }, ImGuiCond_Appearing); //Auto center
    ImGui::SetNextWindowSize({ 500.0f, 450.0f }, ImGuiCond_FirstUseEver);
    if (!ImGui::BeginPopupModal("Settings", open))
    {
        ImGui::EndPopup();
        return;
    }

    fonts->FontL.Push();
    ImGui::Text(ICON_FA_COG " Settings");
    fonts->FontL.Pop();
    ImGui::Separator();

    //Draw settings
    Config* config = Config::Get();
    for (CVar* var : CVar::Instances)
    {
        //Only draw settings with <ShowInSettings>True</ShowInSettings>
        if (!var->ShowInSettings)
            continue;

        auto drawTooltip = [&]()
        {
            gui::TooltipOnPrevious(var->Description, fonts->FontDefault.GetFont());
        };

        switch (var->Type)
        {
        case ConfigType::Int:
            if (var->Min && var->Max) //User slider if min-max is provided
            {
                i32 min = (i32)var->Min.value();
                i32 max = (i32)var->Max.value();
                if (ImGui::SliderInt(var->Name.c_str(), &std::get<i32>(var->Value), min, max))
                    config->Save();
            }
            else
            {
                if (ImGui::InputInt(var->Name.c_str(), &std::get<i32>(var->Value)))
                    config->Save();

            }
            drawTooltip();
            break;
        case ConfigType::Uint:
            if (var->Min && var->Max) //User slider if min-max is provided
            {
                u32 min = (u32)var->Min.value();
                u32 max = (u32)var->Max.value();
                if (ImGui::SliderScalar(var->Name.c_str(), ImGuiDataType_U32, (void*)&std::get<u32>(var->Value), (void*)&min, (void*)&max))
                    config->Save();
            }
            else
            {
                if (ImGui::InputScalar(var->Name.c_str(), ImGuiDataType_U32, &std::get<u32>(var->Value)))
                    config->Save();
            }
            drawTooltip();
            break;
        case ConfigType::Float:
            if (var->Min && var->Max) //User slider if min-max is provided
            {
                if (ImGui::SliderFloat(var->Name.c_str(), (f32*)&std::get<f32>(var->Value), var->Min.value(), var->Max.value()))
                    config->Save();
            }
            else
            {
                if (ImGui::InputFloat(var->Name.c_str(), &std::get<f32>(var->Value)))
                    config->Save();
            }
            drawTooltip();
            break;
        case ConfigType::Bool:
            if (ImGui::Checkbox(var->Name.c_str(), &std::get<bool>(var->Value)))
                config->Save();

            drawTooltip();
            break;
        case ConfigType::String:
            if (var->IsFilePath)
            {
                gui::LabelAndValue(var->Name, std::get<string>(var->Value));
                drawTooltip();
                ImGui::SameLine();
                if (ImGui::Button("Browse..."))
                {
                    std::optional<string> output = OpenFile();
                    if (output)
                    {
                        var->Value = output.value();
                        config->Save();
                    }
                }
            }
            else if (var->IsFolderPath)
            {
                gui::LabelAndValue(var->Name, std::get<string>(var->Value));
                drawTooltip();
                ImGui::SameLine();
                if (ImGui::Button("Browse..."))
                {
                    std::optional<string> output = OpenFolder();
                    if (output)
                    {
                        var->Value = output.value();
                        config->Save();
                    }
                }
            }
            else
            {
                if (ImGui::InputText(var->Name, std::get<string>(var->Value)))
                    config->Save();

                drawTooltip();
            }
            break;
        case ConfigType::List:
            if (ImGui::BeginListBox(var->Name.c_str()))
            {
                drawTooltip();
                u32 i = 0;
                auto& values = std::get<std::vector<string>>(var->Value);
                for (auto& value : values)
                {
                    if (ImGui::InputText(fmt::format("##ListBoxItem{}-{}", (u64)var, i).c_str(), value))
                        config->Save();

                    i++;
                }

                ImGui::EndListBox();
            }
            break;
        case ConfigType::Invalid:
        default:
            break;
        }
    }

    ImGui::EndPopup();
}
